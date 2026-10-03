#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

namespace {

/// An on/off switch: its label at the left, a pill-shaped track at the right with a knob that
/// sits left when off and right when on. A click anywhere on the widget flips it.
class SwitchWidget final : public Widget {
public:
    SwitchWidget(std::string label, bool on) :
        m_label(std::move(label)),
        m_on(on) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f text = context.textSize(m_label, Switch::Label);
        const float knob = widgets::knobSize(sizes);
        const float track = knob * widgets::switchAspect;
        return {
            .min = { track + text.x * 0.5f, std::max(knob, text.y) },
            .preferred = { text.x + sizes.gap.x + track, sizes.rowHeight },
            .max = sf::Vector2f(widgets::anyWidth, sizes.rowHeight * widgets::tallest),
        };
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const auto* release = event.getIf<PointerReleased>();
        if (release == nullptr || release->button != sf::Mouse::Button::Left) {
            return event.is<PointerPressed>();
        }
        if (FloatRect({ 0.f, 0.f }, context.size()).contains(context.local(release->pointer))) {
            m_on = !m_on;
            context.changeValue(m_on);
            context.markDirty();
        }
        return true;
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const float knob = std::min(widgets::knobSize(style.sizes()), size.y);
        const FloatRect track(
            size.x - knob * widgets::switchAspect, (size.y - knob) * 0.5f, knob * widgets::switchAspect, knob
        );

        painter.text(
            FloatRect(0.f, 0.f, std::max(track.left() - style.sizes().gap.x, 0.f), size.y),
            m_label,
            style.part(Switch::Label)
        );

        // On: the track takes the accent look, and the knob what stands out on it.
        const State on = m_on ? State::Active : State::Normal;
        PartStyle trackStyle = style.part(Switch::Track, on);
        trackStyle.radius = fullyRound;
        painter.box(track, trackStyle);

        const float inset = std::max(trackStyle.contentInset(), knob * 0.12f);
        const float diameter = knob - inset * 2.f;
        // TODO(#43): slide the knob over a moment, through the widget's update step.
        const float x = m_on ? track.right() - inset - diameter : track.left() + inset;
        PartStyle knobStyle = style.part(Switch::Knob, on);
        knobStyle.radius = fullyRound;
        painter.box(FloatRect(x, track.top() + inset, diameter, diameter), knobStyle);
    }

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Bool; }
    [[nodiscard]] bool editsValue() const override { return true; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_on); }
    void setValue(const Value& value) override {
        if (const auto* on = std::get_if<bool>(&value)) {
            m_on = *on;
        }
    }

private:
    std::string m_label;
    bool m_on;
};

} // namespace

Switch::Switch(std::string switchName, SwitchOptions switchOptions) :
    name(std::move(switchName)),
    options(std::move(switchOptions)) {}

Switch::Switch(std::string switchName, Param<bool>& value, SwitchOptions switchOptions) :
    name(std::move(switchName)),
    options(std::move(switchOptions)),
    binding(AnyBinding(value)) {}

Switch::Switch(std::string switchName, BoolBinding& value, SwitchOptions switchOptions) :
    name(std::move(switchName)),
    options(std::move(switchOptions)),
    binding(AnyBinding(value)) {}

std::unique_ptr<Widget> Switch::create() const {
    return std::make_unique<SwitchWidget>(options.label.empty() ? name : options.label, options.initial);
}

} // namespace atpl
