#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <utility>

namespace atpl {

namespace {

/// A push button: a face with its label in the middle. It reports a press when the pointer is
/// released over it after being pressed on it; a press that is dragged away and released
/// elsewhere does nothing.
class ButtonWidget final : public Widget {
public:
    explicit ButtonWidget(std::string label) :
        m_label(std::move(label)) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f text = context.textSize(m_label, Button::Label);
        return {
            .min = { text.x + sizes.padding.x, text.y + 4.f },
            .preferred = { text.x + sizes.padding.x * 2.f, sizes.rowHeight },
            .max = sf::Vector2f(widgets::anyWidth, sizes.rowHeight * widgets::tallest),
        };
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const auto* release = event.getIf<PointerReleased>();
        if (release == nullptr || release->button != sf::Mouse::Button::Left) {
            return event.is<PointerPressed>(); // a press is ours; what it means shows on release
        }
        if (FloatRect({ 0.f, 0.f }, context.size()).contains(context.local(release->pointer))) {
            context.press(); // also sets a bound `Param<bool>`
        }
        return true;
    }

    void paint(Painter& painter, const Style& style) const override {
        const FloatRect face({ 0.f, 0.f }, painter.size());
        painter.box(face, style.part(Button::Face));
        painter.text(face, m_label, style.part(Button::Label), Align::Center);
    }

    // A button can be bound to a `Param<bool>`, which it sets on every press.
    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Bool; }
    [[nodiscard]] bool editsValue() const override { return true; }

private:
    std::string m_label;
};

} // namespace

Button::Button(std::string buttonName, ButtonOptions buttonOptions) :
    name(std::move(buttonName)),
    options(std::move(buttonOptions)) {}

Button::Button(std::string buttonName, Param<bool>& pressed, ButtonOptions buttonOptions) :
    name(std::move(buttonName)),
    options(std::move(buttonOptions)),
    binding(AnyBinding(pressed)) {}

std::unique_ptr<Widget> Button::create() const {
    return std::make_unique<ButtonWidget>(options.label.empty() ? name : options.label);
}

} // namespace atpl
