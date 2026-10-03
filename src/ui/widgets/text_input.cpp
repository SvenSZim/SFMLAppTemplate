#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"
#include "ui/widgets/utf8.hpp"

#include <SFML/System/String.hpp>
#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace atpl {

namespace {

/// A line of editable text: its label above, the field below, with a steady cursor while it has
/// the focus. Text is kept as code points.
class TextInputWidget final : public Widget {
public:
    TextInputWidget(std::string label, TextInputOptions options) :
        m_label(std::move(label)),
        m_options(std::move(options)) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const sf::Vector2f label = context.textSize(m_label, TextInput::Label);
        const sf::Vector2f content = context.textSize("Ag", TextInput::Content);
        const float placeholder = context.textSize(m_options.placeholder, TextInput::Placeholder).x;
        return widgets::labelledField(context.sizes(), label, { std::max(placeholder, label.x), content.y });
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const auto width = [&](std::u32string_view text) {
            return context.textSize(widgets::encodeUtf8(text), TextInput::Content).x;
        };
        const FloatRect area = textArea(context.size(), context.textSize(m_label, TextInput::Label).y, context.sizes());

        if (const auto* press = event.getIf<PointerPressed>()) {
            if (press->button == sf::Mouse::Button::Left) {
                context.requestFocus();
                m_cursor = cursorAt(context.local(press->pointer).x - area.left(), width);
                context.markDirty();
            }
            return true;
        }
        if (const auto* typed = event.getIf<TextEntered>()) {
            if (widgets::isPrintable(typed->character)) {
                insert(std::u32string(1, typed->character), context);
            }
            return true;
        }
        if (const auto* key = event.getIf<KeyPressed>()) {
            handleKey(*key, context);
            keepCursorVisible(area.width(), width);
            return true;
        }
        return event.is<PointerReleased>() || event.is<PointerMoved>();
    }

    void focusLost(InputContext& context) override {
        if (std::exchange(m_unreported, false)) {
            context.changeValue(Value(widgets::encodeUtf8(m_text)), true); // the editing is over
        }
        context.markDirty();
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const PartStyle labelStyle = style.part(TextInput::Label);
        const float labelHeight = painter.textSize(m_label, labelStyle).y;
        painter.text(FloatRect(0.f, 0.f, size.x, labelHeight), m_label, labelStyle);

        const PartStyle fieldStyle = style.part(TextInput::Field);
        painter.box(widgets::fieldBelow(size, labelHeight), fieldStyle);

        const FloatRect area = textArea(size, labelHeight, style.sizes());
        if (m_text.empty()) {
            painter.text(area, m_options.placeholder, style.part(TextInput::Placeholder));
        }
        const PartStyle content = style.part(TextInput::Content);
        const auto width = [&](std::u32string_view text) {
            return painter.textSize(widgets::encodeUtf8(text), content).x;
        };
        keepCursorVisible(area.width(), width);

        // What fits from the first visible character on.
        std::size_t end = m_first;
        while (end < m_text.size() &&
               width(std::u32string_view(m_text).substr(m_first, end + 1 - m_first)) <= area.width()) {
            ++end;
        }
        const std::u32string_view shown = std::u32string_view(m_text).substr(m_first, end - m_first);
        painter.text(
            FloatRect(area.left(), area.top(), area.width() + 1.f, area.height()), widgets::encodeUtf8(shown), content
        );

        if (has(style.state(), State::Focused)) {
            const float x = area.left() + width(std::u32string_view(m_text).substr(m_first, m_cursor - m_first));
            const float height = std::min(painter.textSize("Ag", content).y * 1.2f, area.height());
            const float thickness = std::max(std::round(style.sizes().text * 1.5f), 1.f);
            PartStyle cursor = style.part(TextInput::Cursor);
            cursor.radius = 0.f;
            painter.box(FloatRect(x, area.top() + (area.height() - height) * 0.5f, thickness, height), cursor);
        }
    }

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Text; }
    [[nodiscard]] bool editsValue() const override { return true; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(widgets::encodeUtf8(m_text)); }

    void setValue(const Value& value) override {
        if (const auto* text = std::get_if<std::string>(&value)) {
            m_text = widgets::decodeUtf8(*text);
            m_text.resize(std::min(m_text.size(), m_options.maxLength));
            m_cursor = m_text.size();
            m_first = std::min(m_first, m_text.size());
        }
    }

private:
    using Width = std::function<float(std::u32string_view)>;

    /// Where the text goes: inside the field, a little in from its sides.
    [[nodiscard]] static FloatRect textArea(sf::Vector2f size, float labelHeight, const Sizes& sizes) {
        const FloatRect field = widgets::fieldBelow(size, labelHeight);
        const float inset = std::max(sizes.padding.x * 0.6f, 2.f);
        return { field.left() + inset, field.top(), std::max(field.width() - inset * 2.f, 0.f), field.height() };
    }

    /// The position between characters nearest to `x`, measured from the first visible one.
    [[nodiscard]] std::size_t cursorAt(float x, const Width& width) const {
        std::size_t best = m_first;
        float bestDistance = std::abs(x);
        for (std::size_t i = m_first + 1; i <= m_text.size(); ++i) {
            const float distance = std::abs(width(std::u32string_view(m_text).substr(m_first, i - m_first)) - x);
            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    }

    /// Scrolls so that the cursor is inside `room`.
    void keepCursorVisible(float room, const Width& width) const {
        m_first = std::min(m_first, m_cursor);
        while (m_first < m_cursor && width(std::u32string_view(m_text).substr(m_first, m_cursor - m_first)) > room) {
            ++m_first;
        }
    }

    void insert(std::u32string_view text, InputContext& context) {
        const std::size_t room = m_options.maxLength - std::min(m_text.size(), m_options.maxLength);
        const std::u32string_view taken = text.substr(0, room);
        if (taken.empty()) {
            return;
        }
        m_text.insert(m_cursor, taken);
        m_cursor += taken.size();
        changed(context);
    }

    void changed(InputContext& context) {
        m_unreported = true;
        context.changeValue(Value(widgets::encodeUtf8(m_text)), false);
        context.markDirty();
    }

    void handleKey(const KeyPressed& key, InputContext& context) {
        using Key = sf::Keyboard::Key;
        switch (key.key) {
            case Key::Left:
                m_cursor = m_cursor > 0 ? m_cursor - 1 : 0;
                break;
            case Key::Right:
                m_cursor = std::min(m_cursor + 1, m_text.size());
                break;
            case Key::Home:
                m_cursor = 0;
                break;
            case Key::End:
                m_cursor = m_text.size();
                break;
            case Key::Backspace:
                if (m_cursor > 0) {
                    m_text.erase(--m_cursor, 1);
                    changed(context);
                }
                return;
            case Key::Delete:
                if (m_cursor < m_text.size()) {
                    m_text.erase(m_cursor, 1);
                    changed(context);
                }
                return;
            case Key::Enter:
            case Key::Escape:
                context.releaseFocus(); // reports the final value
                return;
            case Key::V:
                if (key.modifiers.control) {
                    std::u32string pasted;
                    for (const char32_t code : sf::Clipboard::getString()) {
                        if (widgets::isPrintable(code)) {
                            pasted.push_back(code); // one line: line breaks are left out
                        }
                    }
                    insert(pasted, context);
                }
                return;
            default:
                return;
        }
        context.markDirty(); // the cursor moved
    }

    std::string m_label;
    TextInputOptions m_options;
    std::u32string m_text;
    std::size_t m_cursor = 0;
    mutable std::size_t m_first = 0; ///< The first character shown: scrolled with the cursor.
    bool m_unreported = false;       ///< Changed since the last final report.
};

} // namespace

TextInput::TextInput(std::string inputName, TextInputOptions inputOptions) :
    name(std::move(inputName)),
    options(std::move(inputOptions)) {}

TextInput::TextInput(std::string inputName, Param<std::string>& value, TextInputOptions inputOptions) :
    name(std::move(inputName)),
    options(std::move(inputOptions)),
    binding(AnyBinding(value)) {}

TextInput::TextInput(std::string inputName, TextBinding& value, TextInputOptions inputOptions) :
    name(std::move(inputName)),
    options(std::move(inputOptions)),
    binding(AnyBinding(value)) {}

std::unique_ptr<Widget> TextInput::create() const {
    if (options.maxLength == 0) {
        throw SetupError("text input \"" + name + "\": maxLength must be at least 1");
    }
    return std::make_unique<TextInputWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
