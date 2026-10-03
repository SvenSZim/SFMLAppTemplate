#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>

namespace atpl {

namespace {

/// A slider: its label at the top left, its value at the top right, and below them the track,
/// filled up to the knob.
///
/// Dragging anywhere on the track row moves the knob there and follows the pointer; the value
/// is reported while dragging (`final` false) and once more on release. The wheel moves it by a
/// step, or by a hundredth of the range. With `step` set, values snap to it.
class SliderWidget final : public Widget {
public:
    SliderWidget(std::string label, SliderOptions options) :
        m_label(std::move(label)),
        m_options(std::move(options)),
        m_value(snapped(m_options.initial)) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f label = context.textSize(m_label, Slider::Label);
        const sf::Vector2f value = context.textSize(widestValue(), Slider::ValueText);
        const float text = std::max(label.y, value.y);
        const float knob = widgets::knobSize(sizes);
        const float wanted = text + 2.f + knob;
        return {
            .min = { std::max(label.x, value.x) + knob * 2.f, text + knob * widgets::trackShare },
            .preferred = { label.x + sizes.gap.x + value.x + knob * 4.f, wanted },
            .max = sf::Vector2f(widgets::anyWidth, wanted * widgets::tallest),
        };
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const Sizes& sizes = context.sizes();
        if (const auto* press = event.getIf<PointerPressed>()) {
            if (press->button != sf::Mouse::Button::Left) {
                return true;
            }
            const sf::Vector2f local = context.local(press->pointer);
            if (local.y < trackRow(context.size(), sizes).top()) {
                return true; // on the label: nothing to move
            }
            m_dragging = true;
            moveTo(valueAt(local.x, context.size(), sizes), false, context);
            return true;
        }
        if (const auto* move = event.getIf<PointerMoved>()) {
            if (m_dragging) {
                moveTo(valueAt(context.local(move->pointer).x, context.size(), sizes), false, context);
            }
            return true;
        }
        if (const auto* release = event.getIf<PointerReleased>()) {
            if (m_dragging && release->button == sf::Mouse::Button::Left) {
                m_dragging = false;
                context.changeValue(m_value, true); // the end of the interaction
            }
            return true;
        }
        if (const auto* wheel = event.getIf<Scrolled>()) {
            const double nudge = m_options.step > 0.0 ? m_options.step : (m_options.max - m_options.min) / 100.0;
            moveTo(m_value + nudge * static_cast<double>(wheel->delta), true, context);
            return true;
        }
        return false;
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const Sizes& sizes = style.sizes();
        const FloatRect row = trackRow(size, sizes);
        const FloatRect text(0.f, 0.f, size.x, row.top());
        painter.text(text, m_label, style.part(Slider::Label));
        painter.text(text, formatted(m_value), style.part(Slider::ValueText), Align::Right);

        const float knob = row.height();
        const float trackHeight = std::max(std::round(knob * widgets::trackShare), 2.f);
        const FloatRect track(row.left(), row.top() + (knob - trackHeight) * 0.5f, row.width(), trackHeight);
        PartStyle trackStyle = style.part(Slider::Track);
        trackStyle.radius = fullyRound;
        painter.box(track, trackStyle);

        // The fill runs inside the track, up to the knob's centre.
        const float centre = knobCentre(row);
        const float inset = trackStyle.contentInset();
        PartStyle fill = style.part(Slider::Fill);
        fill.radius = fullyRound;
        painter.box(
            FloatRect(
                track.left() + inset,
                track.top() + inset,
                std::max(centre - track.left() - inset, 0.f),
                track.height() - inset * 2.f
            ),
            fill
        );

        // Ticks, if the theme shows them: at every step, or at tenths for a continuous slider.
        const PartStyle ticks = style.part(Slider::Ticks);
        if (ticks.shown) {
            const double range = m_options.max - m_options.min;
            const int count = m_options.step > 0.0 ? static_cast<int>(std::round(range / m_options.step)) : 10;
            if (count > 0 && count <= 100) {
                for (int i = 0; i <= count; ++i) {
                    const float x = positionOf(m_options.min + range * i / count, row);
                    painter.line({ x, track.bottom() + 2.f }, { x, row.bottom() }, ticks);
                }
            }
        }

        PartStyle knobStyle = style.part(Slider::Knob);
        knobStyle.radius = fullyRound;
        painter.box(FloatRect(centre - knob * 0.5f, row.top(), knob, knob), knobStyle);
    }

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Number; }
    [[nodiscard]] bool editsValue() const override { return true; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_value); }
    void setValue(const Value& value) override {
        if (const auto* number = std::get_if<double>(&value)) {
            m_value = snapped(*number);
        }
    }

private:
    /// The row of the track and the knob: the bottom of the widget, one knob high.
    [[nodiscard]] static FloatRect trackRow(sf::Vector2f size, const Sizes& sizes) {
        const float knob = std::min(widgets::knobSize(sizes), size.y);
        return { 0.f, size.y - knob, size.x, knob };
    }

    /// The knob travels between half a knob from either end.
    [[nodiscard]] float positionOf(double value, const FloatRect& row) const {
        const float half = row.height() * 0.5f;
        const double share = (value - m_options.min) / (m_options.max - m_options.min);
        return row.left() + half +
               static_cast<float>(std::clamp(share, 0.0, 1.0)) * std::max(row.width() - half * 2.f, 0.f);
    }

    [[nodiscard]] float knobCentre(const FloatRect& row) const { return positionOf(m_value, row); }

    [[nodiscard]] double valueAt(float x, sf::Vector2f size, const Sizes& sizes) const {
        const FloatRect row = trackRow(size, sizes);
        const float half = row.height() * 0.5f;
        const float travel = std::max(row.width() - half * 2.f, 1.f);
        const double share = std::clamp(static_cast<double>((x - row.left() - half) / travel), 0.0, 1.0);
        return snapped(m_options.min + share * (m_options.max - m_options.min));
    }

    [[nodiscard]] double snapped(double value) const {
        double result = std::clamp(value, m_options.min, m_options.max);
        if (m_options.step > 0.0) {
            result = m_options.min + std::round((result - m_options.min) / m_options.step) * m_options.step;
            result = std::clamp(result, m_options.min, m_options.max);
        }
        return result;
    }

    void moveTo(double value, bool final, InputContext& context) {
        const double next = snapped(value);
        if (next != m_value) {
            m_value = next;
            context.markDirty();
            context.changeValue(m_value, final);
        }
    }

    [[nodiscard]] std::string formatted(double value) const {
        return std::vformat(m_options.format, std::make_format_args(value));
    }

    /// The value text that takes the most room, for measuring.
    [[nodiscard]] std::string widestValue() const {
        const std::string low = formatted(m_options.min);
        const std::string high = formatted(m_options.max);
        return low.size() > high.size() ? low : high;
    }

    std::string m_label;
    SliderOptions m_options;
    double m_value;
    bool m_dragging = false;
};

} // namespace

Slider::Slider(std::string sliderName, SliderOptions sliderOptions) :
    name(std::move(sliderName)),
    options(std::move(sliderOptions)) {}

Slider::Slider(std::string sliderName, NumberBinding& value, SliderOptions sliderOptions) :
    name(std::move(sliderName)),
    options(std::move(sliderOptions)),
    binding(AnyBinding(value)) {}

std::unique_ptr<Widget> Slider::create() const {
    const std::string who = "slider \"" + name + "\"";
    if (!(options.max > options.min)) {
        throw SetupError(
            who + ": max (" + std::format("{:g}", options.max) + ") must be above min (" +
            std::format("{:g}", options.min) + ")"
        );
    }
    if (!(options.step >= 0.0)) {
        throw SetupError(who + ": step must not be negative");
    }
    try {
        double probe = options.min;
        static_cast<void>(std::vformat(options.format, std::make_format_args(probe)));
    } catch (const std::format_error& error) {
        throw SetupError(who + ": format \"" + options.format + "\" cannot show a number: " + error.what());
    }
    return std::make_unique<SliderWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
