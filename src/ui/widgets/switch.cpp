#include "atpl/core/easing.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

namespace {

/// An on/off switch: its label at the left, a pill-shaped track at the right with a knob that
/// sits left when off and right when on. A click anywhere on the widget flips it; the knob then
/// slides over, and the track's look fades to the other one, over the theme's toggle time.
class SwitchWidget final : public Widget {
public:
    SwitchWidget(std::string label, bool on) :
        m_label(std::move(label)),
        m_on(on),
        m_position(on ? 1.f : 0.f) {}

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

    // The knob moves at an even pace; painting eases it in and out (`smoothStep`), so turning
    // back halfway does not jump. Its first update puts it where its value says, so a switch
    // whose bound value differs from its initial one does not slide when the UI starts.
    void update(float dt, UpdateContext& context) override {
        const float target = m_on ? 1.f : 0.f;
        if (!std::exchange(m_started, true) || context.motion().toggle <= 0.f) {
            if (m_position != target) {
                m_position = target;
                context.markDirty();
            }
            return;
        }
        if (m_position == target) {
            return;
        }
        const float step = dt / context.motion().toggle;
        m_position = m_position < target ? std::min(m_position + step, target) : std::max(m_position - step, target);
        context.markDirty();
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

        // On: the track takes the accent look, and the knob what stands out on it. On the way,
        // both lie between their looks.
        const float on = smoothStep(m_position);
        PartStyle trackStyle = style.part(Switch::Track, State::Active, on);
        trackStyle.radius = fullyRound;
        painter.box(track, trackStyle);

        const float inset = std::max(trackStyle.contentInset(), knob * 0.12f);
        const float diameter = knob - inset * 2.f;
        const float left = track.left() + inset;
        const float x = left + (track.right() - inset - diameter - left) * on;
        PartStyle knobStyle = style.part(Switch::Knob, State::Active, on);
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
    float m_position;       ///< Where the knob is: 0 off, 1 on, between while it slides.
    bool m_started = false; ///< Whether `update` ran once.
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
