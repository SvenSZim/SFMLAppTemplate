#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/number_format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// How many samples a graph shows at most when its options do not say.
constexpr std::size_t defaultSamples = 1024;

// The shadow and the grid stay in the background: they take their part's colour, at this much
// of its opacity. (The shadow then fades further towards the reference line.)
constexpr float shadowOpacity = 0.45f;
constexpr float gridOpacity = 0.35f;

[[nodiscard]] PartStyle fainter(PartStyle style, float share) {
    style.color.a = static_cast<std::uint8_t>(static_cast<float>(style.color.a) * share);
    return style;
}

/// What the value axis covers, in the axis's own units (powers of ten on a logarithmic axis).
struct ValueRange {
    float low = 0.f;
    float high = 1.f;
    float reference = 0.f; ///< Where the reference line is: zero, or the average.
};

/// A line graph: its label above, at the right the newest value if the theme shows it; below,
/// the plot: a background, the reference line (zero or the average), the curve, and if the theme
/// shows them the shadow, a grid, axis lines and axis labels.
class GraphWidget final : public Widget {
public:
    GraphWidget(std::string label, GraphOptions options) :
        m_label(std::move(label)),
        m_options(std::move(options)),
        m_capacity(m_options.samples > 0 ? m_options.samples : defaultSamples) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const float label = context.textSize(m_label, Graph::Label).y;
        const float plot = m_options.height > 0.f ? m_options.height * sizes.scale.y : sizes.rowHeight * 4.f;
        return {
            .min = { 120.f, label + sizes.rowHeight * 2.f },
            .preferred = { 240.f, label + 4.f + plot },
        };
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const PartStyle labelStyle = style.part(Graph::Label);
        const PartStyle valueStyle = style.part(Graph::Value);
        const float labelHeight = std::max(painter.textSize(m_label, labelStyle).y, labelStyle.textSize) + 4.f;

        read();
        const FloatRect top(0.f, 0.f, size.x, labelHeight);
        painter.text(top, m_label, labelStyle);
        if (!m_ys.empty()) {
            painter.text(top, widgets::formatNumber(m_options.format, m_ys.back()), valueStyle, Align::Right);
        }

        const PartStyle background = style.part(Graph::Background);
        const FloatRect box(0.f, labelHeight, size.x, std::max(size.y - labelHeight, 0.f));
        painter.box(box, background);

        const ValueRange range = rangeOf();
        const std::array<std::string, 3> yLabels = { axisLabel(range.high),
                                                     axisLabel((range.low + range.high) * 0.5f),
                                                     axisLabel(range.low) };
        const std::array<std::string, 3> xLabels = xAxisLabels();

        // The plot: inside the box, leaving room for the axis labels if they are shown.
        const PartStyle labels = style.part(Graph::AxisLabels);
        const float inset = background.contentInset() + 4.f;
        float left = box.left() + inset;
        float bottom = box.bottom() - inset;
        if (labels.shown) {
            float widest = 0.f;
            for (const std::string& text : yLabels) {
                widest = std::max(widest, painter.textSize(text, labels).x);
            }
            left += widest + 6.f;
            bottom -= labels.textSize + 4.f;
        }
        const FloatRect plot(
            left,
            box.top() + inset,
            std::max(box.right() - inset - left, 1.f),
            std::max(bottom - box.top() - inset, 1.f)
        );
        const auto yOf = [&](float axisValue) {
            const float share = (axisValue - range.low) / (range.high - range.low);
            return plot.bottom() - std::clamp(share, 0.f, 1.f) * plot.height();
        };

        // The grid, at the label positions: three lines across, four down.
        const PartStyle grid = fainter(style.part(Graph::Grid), gridOpacity);
        if (grid.shown) {
            for (int i = 0; i <= 2; ++i) {
                const float y = plot.top() + plot.height() * static_cast<float>(i) * 0.5f;
                painter.line({ plot.left(), y }, { plot.right(), y }, grid);
            }
            for (int i = 0; i <= 4; ++i) {
                const float x = plot.left() + plot.width() * static_cast<float>(i) * 0.25f;
                painter.line({ x, plot.top() }, { x, plot.bottom() }, grid);
            }
        }

        const float referenceY = yOf(range.reference);
        curve(plot, yOf);
        if (!m_curve.empty()) {
            painter.area(m_curve, referenceY, fainter(style.part(Graph::Shadow), shadowOpacity));
        }
        painter.line({ plot.left(), referenceY }, { plot.right(), referenceY }, style.part(Graph::Baseline));
        if (m_curve.size() >= 2) {
            painter.polyline(m_curve, style.part(Graph::Curve));
        }

        const PartStyle axis = style.part(Graph::Axis);
        painter.line({ plot.left(), plot.top() }, { plot.left(), plot.bottom() }, axis);
        painter.line({ plot.left(), plot.bottom() }, { plot.right(), plot.bottom() }, axis);

        if (labels.shown) {
            const float textHeight = labels.textSize;
            const float labelWidth = plot.left() - box.left() - inset - 6.f;
            for (int i = 0; i <= 2; ++i) {
                const float y = plot.top() + plot.height() * static_cast<float>(i) * 0.5f;
                painter.text(
                    FloatRect(box.left() + inset, y - textHeight * 0.5f, labelWidth, textHeight),
                    yLabels[static_cast<std::size_t>(i)],
                    labels,
                    Align::Right
                );
            }
            const float y = plot.bottom() + 2.f;
            const float third = plot.width() / 3.f;
            painter.text(FloatRect(plot.left(), y, third, textHeight + 2.f), xLabels[0], labels, Align::Left);
            painter.text(FloatRect(plot.left() + third, y, third, textHeight + 2.f), xLabels[1], labels, Align::Center);
            painter.text(FloatRect(plot.right() - third, y, third, textHeight + 2.f), xLabels[2], labels, Align::Right);
        }
    }

    [[nodiscard]] bool reactsToPointer() const override { return false; }
    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Series; }

    void setSeries(const SeriesBinding* source) override {
        m_source = source;
        // Room for the window, made once: nothing is allocated while drawing.
        m_ys.reserve(m_capacity);
        m_points.reserve(m_capacity);
        m_xs.reserve(m_capacity);
        m_curve.reserve(m_capacity);
    }

private:
    /// Reads the newest samples or points from the source into the buffers.
    void read() const {
        m_ys.clear();
        m_xs.clear();
        if (m_source == nullptr) {
            return;
        }
        if (m_source->hasPoints()) {
            m_points.resize(m_capacity);
            const std::size_t count = m_source->readPoints(m_points);
            for (std::size_t i = 0; i < count; ++i) {
                m_xs.push_back(m_points[i].x);
                m_ys.push_back(m_points[i].y);
            }
        } else {
            m_ys.resize(m_capacity);
            m_ys.resize(m_source->read(m_ys));
        }
    }

    /// A value in the axis's own units: itself, or its power of ten on a logarithmic axis.
    [[nodiscard]] float onAxis(float value) const {
        if (!m_options.logarithmic) {
            return value;
        }
        const float smallest = m_options.min.value_or(1.0e-3f);
        return std::log10(std::max(value, smallest));
    }

    [[nodiscard]] ValueRange rangeOf() const {
        float low = std::numeric_limits<float>::max();
        float high = std::numeric_limits<float>::lowest();
        double sum = 0.0;
        for (const float value : m_ys) {
            const float v = onAxis(value);
            low = std::min(low, v);
            high = std::max(high, v);
            sum += v;
        }
        if (m_ys.empty()) {
            low = 0.f;
            high = 1.f;
        }

        ValueRange range;
        if (m_options.base == GraphBase::Average && !m_ys.empty()) {
            const auto average = static_cast<float>(sum / static_cast<double>(m_ys.size()));
            const float half = std::max({ high - average, average - low, 1.0e-6f });
            range = { average - half, average + half, average };
        } else if (m_options.logarithmic) {
            // Whole powers of ten at both ends: 0.1 to 1000, not 0.13 to 879.
            range = { std::floor(low), std::max(std::ceil(high), std::floor(low) + 1.f), std::floor(low) };
        } else {
            const float bottom = std::min(low, 0.f);
            range = { bottom, std::max(high, bottom + 1.0e-6f), 0.f };
        }

        if (m_options.min.has_value()) {
            range.low = onAxis(*m_options.min);
        }
        if (m_options.max.has_value()) {
            range.high = onAxis(*m_options.max);
        }
        if (!(range.high > range.low)) {
            range.high = range.low + 1.f;
        }
        range.reference = std::clamp(range.reference, range.low, range.high);
        return range;
    }

    [[nodiscard]] std::string axisLabel(float axisValue) const {
        const double value = m_options.logarithmic ? std::pow(10.0, static_cast<double>(axisValue)) : axisValue;
        return widgets::formatNumber(m_options.format, value);
    }

    /// Labels at the left end, the middle and the right end of the x-axis.
    [[nodiscard]] std::array<std::string, 3> xAxisLabels() const {
        if (!m_xs.empty()) {
            const auto [low, high] = std::minmax_element(m_xs.begin(), m_xs.end());
            return { widgets::formatNumber(m_options.format, *low),
                     widgets::formatNumber(m_options.format, (*low + *high) * 0.5f),
                     widgets::formatNumber(m_options.format, *high) };
        }
        const auto span = static_cast<double>(window() - 1);
        if (m_options.x == GraphX::Time) {
            const double seconds = span * static_cast<double>(m_options.secondsPerSample);
            return { std::format("{:g} s", -seconds), std::format("{:g} s", -seconds * 0.5), "0 s" };
        }
        return { std::format("{:g}", -span), std::format("{:g}", -std::floor(span * 0.5)), "0" };
    }

    /// The curve's points in the plot. Samples: the newest at the right edge, the window spread
    /// over the width. Points: their x values across the range of the data.
    template <typename YOf>
    void curve(const FloatRect& plot, const YOf& yOf) const {
        m_curve.clear();
        if (!m_xs.empty()) {
            const auto [low, high] = std::minmax_element(m_xs.begin(), m_xs.end());
            const float span = std::max(*high - *low, 1.0e-6f);
            for (std::size_t i = 0; i < m_xs.size(); ++i) {
                m_curve.push_back({ plot.left() + (m_xs[i] - *low) / span * plot.width(), yOf(onAxis(m_ys[i])) });
            }
            return;
        }
        const float step = window() > 1 ? plot.width() / static_cast<float>(window() - 1) : 0.f;
        const std::size_t count = m_ys.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto fromNewest = static_cast<float>(count - 1 - i);
            m_curve.push_back({ plot.right() - fromNewest * step, yOf(onAxis(m_ys[i])) });
        }
    }

    /// How many samples the width shows: as many as the options say, or else as many as there are.
    [[nodiscard]] std::size_t window() const {
        return m_options.samples > 0 ? m_options.samples : std::max<std::size_t>(m_ys.size(), 2);
    }

    std::string m_label;
    GraphOptions m_options;
    std::size_t m_capacity; ///< The most samples read.
    const SeriesBinding* m_source = nullptr;

    // Buffers for drawing, kept from paint to paint.
    mutable std::vector<float> m_ys;
    mutable std::vector<float> m_xs;
    mutable std::vector<Point> m_points;
    mutable std::vector<sf::Vector2f> m_curve;
};

} // namespace

Graph::Graph(std::string graphName, GraphOptions graphOptions) :
    name(std::move(graphName)),
    options(std::move(graphOptions)) {}

Graph::Graph(std::string graphName, Series& samples, GraphOptions graphOptions) :
    name(std::move(graphName)),
    options(std::move(graphOptions)),
    binding(AnyBinding(samples)) {}

Graph::Graph(std::string graphName, PointSeries& points, GraphOptions graphOptions) :
    name(std::move(graphName)),
    options(std::move(graphOptions)),
    binding(AnyBinding(points)) {}

Graph::Graph(std::string graphName, SeriesBinding& samples, GraphOptions graphOptions) :
    name(std::move(graphName)),
    options(std::move(graphOptions)),
    binding(AnyBinding(samples)) {}

std::unique_ptr<Widget> Graph::create() const {
    const std::string who = "graph \"" + name + "\"";
    if (options.min.has_value() && options.max.has_value() && !(*options.max > *options.min)) {
        throw SetupError(who + ": max must be above min");
    }
    if (options.logarithmic && options.min.has_value() && !(*options.min > 0.f)) {
        throw SetupError(who + ": a logarithmic axis needs a min above zero");
    }
    if (options.x == GraphX::Time && !(options.secondsPerSample > 0.f)) {
        throw SetupError(who + ": a time axis needs time between samples (secondsPerSample)");
    }
    widgets::requireNumberFormat(who, options.format);
    return std::make_unique<GraphWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
