#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/number_format.hpp"
#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

namespace {

/// A label with a value next to it: the label at the left, muted, the value at the right.
/// Numbers are shown in the widget's format, switches as "on" and "off", choices by their number.
class TextDisplayWidget final : public Widget {
public:
    TextDisplayWidget(std::string label, std::string format, std::string initial) :
        m_label(std::move(label)),
        m_format(std::move(format)),
        m_text(std::move(initial)) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f label = context.textSize(m_label, TextDisplay::Label);
        const sf::Vector2f value = context.textSize(m_text, TextDisplay::ValueText);
        const float text = std::max(label.y, value.y);
        return {
            .min = { label.x * 0.5f + value.x, text },
            .preferred = { label.x + sizes.gap.x + value.x, sizes.rowHeight },
            .max = sf::Vector2f(widgets::anyWidth, sizes.rowHeight * widgets::tallest),
        };
    }

    void paint(Painter& painter, const Style& style) const override {
        const FloatRect all({ 0.f, 0.f }, painter.size());
        painter.text(all, m_label, style.part(TextDisplay::Label));
        painter.text(all, m_text, style.part(TextDisplay::ValueText), Align::Right);
    }

    [[nodiscard]] bool reactsToPointer() const override { return false; }
    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind != ValueKind::Series; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_text); }

    void setValue(const Value& value) override {
        if (const auto* number = std::get_if<double>(&value)) {
            m_text = widgets::formatNumber(m_format, *number);
        } else if (const auto* flag = std::get_if<bool>(&value)) {
            m_text = *flag ? "on" : "off";
        } else if (const auto* index = std::get_if<std::size_t>(&value)) {
            m_text = std::to_string(*index);
        } else if (const auto* text = std::get_if<std::string>(&value)) {
            m_text = *text;
        }
    }

private:
    std::string m_label;
    std::string m_format;
    std::string m_text;
};

} // namespace

TextDisplay::TextDisplay(std::string displayName, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)) {}

TextDisplay::TextDisplay(std::string displayName, TextBinding& value, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)),
    binding(AnyBinding(value)) {}

TextDisplay::TextDisplay(std::string displayName, NumberBinding& value, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)),
    binding(AnyBinding(value)) {}

std::unique_ptr<Widget> TextDisplay::create() const {
    widgets::requireNumberFormat("text display \"" + name + "\"", options.format);
    return std::make_unique<TextDisplayWidget>(
        options.label.empty() ? name : options.label, options.format, options.initial
    );
}

} // namespace atpl
