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
#include <string_view>
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
                // Where the pointer is in the text as it is shown now.
                const Shown shown = visible(area.width(), has(context.state(), State::Focused), width);
                m_cursor = cursorAt(context.local(press->pointer).x - area.left() - shown.textLeft, shown, width);
                context.requestFocus();
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
            static_cast<void>(visible(area.width(), true, width)); // scrolls with the cursor
            return true;
        }
        return event.is<PointerReleased>() || event.is<PointerMoved>();
    }

    void focusLost(InputContext& context) override {
        m_first = 0; // shown from its start again
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
        const bool editing = has(style.state(), State::Focused);
        const Shown shown = visible(area.width(), editing, width);

        // What fits, with dots at the side where text is hidden.
        const std::u32string_view all(m_text);
        const float textWidth = width(all.substr(shown.first, shown.end - shown.first));
        const float left = area.left() + shown.textLeft;
        if (shown.dotsLeft) {
            painter.text(FloatRect(area.left(), area.top(), shown.textLeft + 1.f, area.height()), dots, content);
        }
        painter.text(
            FloatRect(left, area.top(), textWidth + 1.f, area.height()),
            widgets::encodeUtf8(all.substr(shown.first, shown.end - shown.first)),
            content
        );
        if (shown.dotsRight) {
            painter.text(
                FloatRect(left + textWidth, area.top(), area.right() - left - textWidth + 1.f, area.height()),
                dots,
                content
            );
        }

        if (editing) {
            const float x = left + width(all.substr(shown.first, m_cursor - shown.first));
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

    /// What marks hidden text at either side.
    static constexpr std::string_view dots = "...";

    /// The part of the text that is shown, with dots where text is hidden.
    struct Shown {
        std::size_t first = 0; ///< The first character shown.
        std::size_t end = 0;   ///< One past the last.
        float textLeft = 0.f;  ///< Where it starts in the text area: after the dots at the left.
        bool dotsLeft = false;
        bool dotsRight = false;
    };

    /// What of the text is shown in `room`. While editing, it scrolls so that the cursor stays in
    /// view, and dots at the left mark that it has scrolled. Otherwise it is shown from its start.
    /// Either way, dots at the right mark text beyond the end.
    [[nodiscard]] Shown visible(float room, bool editing, const Width& width) const {
        const std::u32string_view all(m_text);
        const auto span = [&](std::size_t from, std::size_t to) { return width(all.substr(from, to - from)); };
        if (span(0, all.size()) <= room) {
            m_first = 0;
            return { .first = 0, .end = all.size() };
        }
        const float dotsWidth = width(U"...");
        const auto reserved = [&](std::size_t first, std::size_t last) {
            return (first > 0 ? dotsWidth : 0.f) + (last < all.size() ? dotsWidth : 0.f);
        };

        if (!editing) {
            m_first = 0;
        } else {
            // The cursor between the dots; and no further scrolled than needed, so that deleting
            // at the end brings the text back from the left.
            m_first = std::min(m_first, m_cursor);
            while (m_first < m_cursor && span(m_first, m_cursor) > room - reserved(m_first, m_cursor)) {
                ++m_first;
            }
            while (m_first > 0 && m_cursor == all.size() &&
                   span(m_first - 1, all.size()) <= room - reserved(m_first - 1, all.size())) {
                --m_first;
            }
        }

        const float left = m_first > 0 ? dotsWidth : 0.f;
        std::size_t end = m_first;
        while (end < all.size() && span(m_first, end + 1) + (end + 1 < all.size() ? dotsWidth : 0.f) <= room - left) {
            ++end;
        }
        if (editing) {
            end = std::max(end, std::min(m_cursor, all.size())); // the cursor is never hidden
        }
        return {
            .first = m_first, .end = end, .textLeft = left, .dotsLeft = m_first > 0, .dotsRight = end < all.size()
        };
    }

    /// Where the text goes: inside the field, a little in from its sides.
    [[nodiscard]] static FloatRect textArea(sf::Vector2f size, float labelHeight, const Sizes& sizes) {
        const FloatRect field = widgets::fieldBelow(size, labelHeight);
        const float inset = std::max(sizes.padding.x * 0.6f, 2.f);
        return { field.left() + inset, field.top(), std::max(field.width() - inset * 2.f, 0.f), field.height() };
    }

    /// The position between the shown characters nearest to `x`, measured from the first shown.
    [[nodiscard]] std::size_t cursorAt(float x, const Shown& shown, const Width& width) const {
        std::size_t best = shown.first;
        float bestDistance = std::abs(x);
        for (std::size_t i = shown.first + 1; i <= shown.end; ++i) {
            const float distance =
                std::abs(width(std::u32string_view(m_text).substr(shown.first, i - shown.first)) - x);
            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
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
    mutable std::size_t m_first = 0; ///< The first character shown while editing: scrolled with the cursor.
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
