#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// The time of day of a moment, as `HH:MM:SS` in local time.
[[nodiscard]] std::string timeOfDay(std::chrono::system_clock::time_point moment) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(moment);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char text[16];
    const std::size_t length = std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return { text, length };
}

/// A box of text lines: the newest of its log at the bottom. Its height is fixed by how many
/// lines it shows; it never changes with the text.
class LogWidget final : public Widget {
public:
    LogWidget(std::string label, LogOptions options) :
        m_label(std::move(label)),
        m_options(options) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f label = context.textSize(m_label, Log::Label);
        const float line = lineHeight(context.textSize("Ag", Log::Text).y);
        const float height =
            label.y + widgets::labelGap + static_cast<float>(m_options.lines) * line + insetOf(sizes).y * 2.f;
        return {
            .min = { sizes.rowHeight * 4.f, height },
            .preferred = { std::max(label.x, sizes.panelWidth - sizes.padding.x * 2.f), height },
            .max = sf::Vector2f(widgets::anyWidth, height), // fixed: text never makes it higher
        };
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const Geometry place = geometry(
            context.size(),
            context.textSize(m_label, Log::Label).y,
            lineHeight(context.textSize("Ag", Log::Text).y),
            context.sizes()
        );
        if (const auto* wheel = event.getIf<Scrolled>()) {
            // A notch moves as far as a panel's would: about a row.
            const float lines = std::max(std::round(context.sizes().rowHeight / place.line), 1.f);
            scrollBack(static_cast<long long>(std::round(wheel->delta * lines)), context);
            return true;
        }
        if (const auto* press = event.getIf<PointerPressed>()) {
            const sf::Vector2f at = context.local(press->pointer);
            if (press->button == sf::Mouse::Button::Left && scrollable() && place.scrollbar.contains(at)) {
                // On the thumb it is dragged from where it was grabbed; beside it, it jumps there.
                const Thumb thumb = thumbOf(place);
                m_grab = at.y >= thumb.top && at.y <= thumb.top + thumb.length ? at.y - thumb.top : thumb.length * 0.5f;
                m_dragging = true;
                context.capturePointer();
                dragTo(at.y, place, context);
            }
            return true;
        }
        if (const auto* move = event.getIf<PointerMoved>()) {
            if (m_dragging) {
                dragTo(context.local(move->pointer).y, place, context);
            }
            return true;
        }
        if (event.is<PointerReleased>()) {
            m_dragging = false;
            return true;
        }
        return false;
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const PartStyle labelStyle = style.part(Log::Label);
        const float labelHeight = painter.textSize(m_label, labelStyle).y;
        painter.text(FloatRect(0.f, 0.f, size.x, labelHeight), m_label, labelStyle);

        const PartStyle text = style.part(Log::Text);
        const Geometry place = geometry(size, labelHeight, lineHeight(painter.textSize("Ag", text).y), style.sizes());
        painter.box(place.box, style.part(Log::Background));
        readLines(place.rows);

        // The lines, from the top: the window of the log that is scrolled to.
        const PartStyle time = style.part(Log::Time);
        const float timeWidth = time.shown ? painter.textSize("00:00:00 ", time).x : 0.f;
        const std::size_t end = m_lines.size() - std::min(m_back, m_lines.size());
        const std::size_t first = end - std::min(end, place.rows);
        float y = place.text.top();
        for (std::size_t i = first; i < end; ++i) {
            const FloatRect row(place.text.left(), y, place.text.width(), place.line);
            if (time.shown) {
                painter.text(
                    FloatRect(row.left(), row.top(), timeWidth, row.height()), timeOfDay(m_lines[i].time), time
                );
            }
            painter.text(
                FloatRect(row.left() + timeWidth, row.top(), std::max(row.width() - timeWidth, 0.f), row.height()),
                m_lines[i].text,
                text
            );
            y += place.line;
        }

        // A log longer than it shows has a scrollbar, as a panel does.
        if (scrollable()) {
            PartStyle bar = style.part(Log::Scrollbar);
            bar.radius = fullyRound;
            bar.borderThickness = 0.f;
            bar.shadow = {};
            const Thumb thumb = thumbOf(place);
            const float x = place.scrollbar.left() + (place.scrollbar.width() - place.barWidth) * 0.5f;
            painter.box(FloatRect(x, thumb.top, place.barWidth, thumb.length), bar);
        }
    }

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Lines; }

    void setLines(const LinesBinding* source) override {
        m_source = source;
        m_back = 0;
        m_seen = source != nullptr ? source->pushed() : 0;
        m_lines.clear();
        m_total = 0;
    }

private:
    /// Where its parts are, in its own coordinates.
    struct Geometry {
        FloatRect box;        ///< The background.
        FloatRect text;       ///< Where the lines go.
        FloatRect scrollbar;  ///< Its track, in the box's right inset; also where the pointer finds it.
        float barWidth = 0.f; ///< How wide its thumb is drawn, in the middle of the track.
        float line = 1.f;     ///< The height of a line.
        std::size_t rows = 1;
    };

    struct Thumb {
        float top = 0.f;
        float length = 0.f;
    };

    [[nodiscard]] static float lineHeight(float textHeight) { return std::max(std::round(textHeight * 1.3f), 1.f); }

    [[nodiscard]] static sf::Vector2f insetOf(const Sizes& sizes) {
        return { std::max(sizes.padding.x * 0.6f, 2.f), std::max(std::round(sizes.padding.y * 0.4f), 2.f) };
    }

    [[nodiscard]] Geometry geometry(sf::Vector2f size, float labelHeight, float line, const Sizes& sizes) const {
        Geometry place;
        place.box = widgets::fieldBelow(size, labelHeight);
        const sf::Vector2f inset = insetOf(sizes);
        place.line = line;
        place.text = FloatRect(
            place.box.left() + inset.x,
            place.box.top() + inset.y,
            std::max(place.box.width() - inset.x * 2.f, 0.f),
            std::max(place.box.height() - inset.y * 2.f, 0.f)
        );
        place.rows = std::max<std::size_t>(static_cast<std::size_t>(std::floor(place.text.height() / line + 0.01f)), 1);
        place.scrollbar = FloatRect(place.box.right() - inset.x, place.text.top(), inset.x, place.text.height());
        place.barWidth = std::min(sizes.scrollbarWidth, inset.x);
        return place;
    }

    /// Reads the lines it may show: those in view and those it is scrolled back past. Lines that
    /// came while it is scrolled back move it further back, so that it keeps showing the same.
    void readLines(std::size_t rows) const {
        if (m_source == nullptr) {
            m_lines.clear();
            m_total = 0;
            return;
        }
        const std::uint64_t pushed = m_source->pushed();
        if (m_back > 0 && pushed > m_seen) {
            m_back += static_cast<std::size_t>(pushed - m_seen);
        }
        m_seen = pushed;
        m_total = m_source->size();
        m_rows = rows;
        m_back = std::min(m_back, m_total - std::min(rows, m_total));
        m_source->read(m_lines, m_back + rows);
    }

    [[nodiscard]] bool scrollable() const { return m_total > m_rows; }

    [[nodiscard]] Thumb thumbOf(const Geometry& place) const {
        const float track = place.scrollbar.height();
        const float length = std::min(
            std::max(
                track * static_cast<float>(m_rows) / static_cast<float>(std::max(m_total, std::size_t{ 1 })), place.line
            ),
            track
        );
        const std::size_t range = m_total - std::min(m_rows, m_total);
        const float share = range > 0 ? 1.f - static_cast<float>(m_back) / static_cast<float>(range) : 1.f;
        return { .top = place.scrollbar.top() + share * (track - length), .length = length };
    }

    /// Moves the view `lines` lines further back (up), or forward for a negative number.
    void scrollBack(long long lines, InputContext& context) {
        const std::size_t range = m_total - std::min(m_rows, m_total);
        const long long back = std::clamp(static_cast<long long>(m_back) + lines, 0LL, static_cast<long long>(range));
        if (static_cast<std::size_t>(back) != m_back) {
            m_back = static_cast<std::size_t>(back);
            context.markDirty();
        }
    }

    void dragTo(float y, const Geometry& place, InputContext& context) {
        const Thumb thumb = thumbOf(place);
        const float travel = place.scrollbar.height() - thumb.length;
        const std::size_t range = m_total - std::min(m_rows, m_total);
        const float share = travel > 0.f ? std::clamp((y - m_grab - place.scrollbar.top()) / travel, 0.f, 1.f) : 1.f;
        const auto back = static_cast<std::size_t>(std::round((1.f - share) * static_cast<float>(range)));
        scrollBack(static_cast<long long>(back) - static_cast<long long>(m_back), context);
    }

    std::string m_label;
    LogOptions m_options;
    const LinesBinding* m_source = nullptr;

    // What it read when it was last painted, and where it is scrolled to.
    mutable std::vector<LogLine> m_lines;
    mutable std::size_t m_back = 0;   ///< How many lines it is scrolled back from the newest; 0 follows.
    mutable std::uint64_t m_seen = 0; ///< The log's count of lines when it last read.
    mutable std::size_t m_total = 0;  ///< How many lines the log has.
    mutable std::size_t m_rows = 1;   ///< How many it shows.

    bool m_dragging = false;
    float m_grab = 0.f;
};

} // namespace

Log::Log(std::string logName, LogOptions logOptions) :
    name(std::move(logName)),
    options(std::move(logOptions)) {}

Log::Log(std::string logName, TextLog& lines, LogOptions logOptions) :
    name(std::move(logName)),
    options(std::move(logOptions)),
    binding(AnyBinding(lines)) {}

Log::Log(std::string logName, LinesBinding& lines, LogOptions logOptions) :
    name(std::move(logName)),
    options(std::move(logOptions)),
    binding(AnyBinding(lines)) {}

std::unique_ptr<Widget> Log::create() const {
    if (options.lines == 0) {
        throw SetupError("log \"" + name + "\": it needs to show at least 1 line");
    }
    return std::make_unique<LogWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
