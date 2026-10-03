#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace atpl {

namespace {

/// A bar that shows how far a number is between `min` and `max`: its label above, the bar below,
/// filled up to the value. Like a slider's track, without a knob.
class ProgressBarWidget final : public Widget {
public:
    ProgressBarWidget(std::string label, ProgressBarOptions options) :
        m_label(std::move(label)),
        m_options(std::move(options)),
        m_value(m_options.min) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f label = context.textSize(m_label, ProgressBar::Label);
        const float wanted = label.y + 4.f + barHeight(sizes);
        return {
            .min = { std::max(label.x, 40.f), label.y + barHeight(sizes) },
            .preferred = { label.x * 2.f, wanted },
            .max = sf::Vector2f(widgets::anyWidth, wanted * widgets::tallest),
        };
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const float bar = std::min(barHeight(style.sizes()), size.y);
        painter.text(FloatRect(0.f, 0.f, size.x, size.y - bar), m_label, style.part(ProgressBar::Label));

        const FloatRect track(0.f, size.y - bar, size.x, bar);
        PartStyle trackStyle = style.part(ProgressBar::Track);
        trackStyle.radius = fullyRound;
        painter.box(track, trackStyle);

        const float inset = trackStyle.contentInset();
        const double share = std::clamp((m_value - m_options.min) / (m_options.max - m_options.min), 0.0, 1.0);
        const float width = (track.width() - inset * 2.f) * static_cast<float>(share);
        if (width > 0.f) {
            PartStyle fill = style.part(ProgressBar::Fill);
            fill.radius = fullyRound;
            painter.box(
                FloatRect(track.left() + inset, track.top() + inset, width, track.height() - inset * 2.f), fill
            );
        }
    }

    [[nodiscard]] bool reactsToPointer() const override { return false; }
    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Number; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_value); }
    void setValue(const Value& value) override {
        if (const auto* number = std::get_if<double>(&value)) {
            m_value = *number;
        }
    }

private:
    [[nodiscard]] static float barHeight(const Sizes& sizes) {
        return std::max(std::round(widgets::knobSize(sizes) * widgets::trackShare), 4.f);
    }

    std::string m_label;
    ProgressBarOptions m_options;
    double m_value;
};

} // namespace

ProgressBar::ProgressBar(std::string barName, ProgressBarOptions barOptions) :
    name(std::move(barName)),
    options(std::move(barOptions)) {}

ProgressBar::ProgressBar(std::string barName, NumberBinding& value, ProgressBarOptions barOptions) :
    name(std::move(barName)),
    options(std::move(barOptions)),
    binding(AnyBinding(value)) {}

std::unique_ptr<Widget> ProgressBar::create() const {
    if (!(options.max > options.min)) {
        throw SetupError(
            "progress bar \"" + name + "\": max (" + std::format("{:g}", options.max) + ") must be above min (" +
            std::format("{:g}", options.min) + ")"
        );
    }
    return std::make_unique<ProgressBarWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
