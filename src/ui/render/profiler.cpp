#include "ui/render/profiler.hpp"

#include "atpl/ui/widget.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace atpl::render {

namespace {

constexpr std::array<std::string_view, Profiler::lineCount> labels = { "frames", "build", "submit", "draw", "rebuilt" };

[[nodiscard]] double milliseconds(Profiler::Clock::duration time) {
    return std::chrono::duration<double, std::milli>(time).count();
}

[[nodiscard]] std::string_view textOf(const std::array<char, 48>& line) {
    return { line.data() };
}

} // namespace

Profiler::Profiler() {
    // The readout is off until somebody asks for it, and being off is not a change to draw.
    m_batch.setVisible(false);
    static_cast<void>(m_batch.takeChanged());
}

void Profiler::Sum::closeFrame() {
    total += pending;
    max = std::max(max, pending);
    pending = 0.;
}

Profiler::Timing Profiler::Sum::over(std::size_t frames) const {
    return { frames > 0 ? total / static_cast<double>(frames) : 0., max };
}

Profiler::Sum& Profiler::sum(Section section) {
    switch (section) {
        case Section::Build:
            return m_build;
        case Section::Submit:
            return m_submit;
        case Section::Show:
            break;
    }
    return m_show;
}

void Profiler::record(Section section, Clock::duration time) {
    sum(section).pending += milliseconds(time);
}

void Profiler::frameDrawn(const FrameStats& stats, std::size_t panelsRebuilt, std::size_t textsBuilt) {
    ++m_frames;
    m_build.closeFrame();
    m_submit.closeFrame();
    m_show.closeFrame();
    m_lastFrame = stats;
    m_panelsRebuilt += panelsRebuilt;
    m_textsBuilt += textsBuilt;
}

void Profiler::frameSkipped() {
    ++m_skipped;
}

void Profiler::dropFrame() {
    m_build.pending = 0.;
    m_submit.pending = 0.;
    m_show.pending = 0.;
}

Profiler::Readout Profiler::take(Clock::time_point now) {
    Readout result;
    if (m_start.has_value()) {
        result.seconds = std::chrono::duration<double>(now - *m_start).count();
    }
    result.frames = m_frames;
    result.skipped = m_skipped;
    result.framesPerSecond = result.seconds > 0. ? static_cast<double>(m_frames) / result.seconds : 0.;
    result.build = m_build.over(m_frames);
    result.submit = m_submit.over(m_frames);
    result.show = m_show.over(m_frames);
    result.lastFrame = m_lastFrame;
    if (m_frames > 0) {
        result.panelsRebuilt = static_cast<double>(m_panelsRebuilt) / static_cast<double>(m_frames);
        result.textsBuilt = static_cast<double>(m_textsBuilt) / static_cast<double>(m_frames);
    }

    // What is pending belongs to a frame that is still being made: it carries over.
    m_start = now;
    m_frames = 0;
    m_skipped = 0;
    m_panelsRebuilt = 0;
    m_textsBuilt = 0;
    m_build.total = m_build.max = 0.;
    m_submit.total = m_submit.max = 0.;
    m_show.total = m_show.max = 0.;
    return result;
}

void Profiler::setVisible(bool visible) {
    if (visible && !m_batch.isVisible()) {
        // Start measuring afresh: what piled up while nobody looked says nothing about now.
        m_start.reset();
        m_refreshed.reset();
    }
    m_batch.setVisible(visible);
}

void Profiler::setLook(const Theme& theme, const Sizes& sizes, PanelColors colors) {
    m_background = theme.resolve(Background, State::Normal, colors, sizes.text);
    m_label = theme.resolve(Label, State::Normal, colors, sizes.text);
    m_value = theme.resolve(Value, State::Normal, colors, sizes.text);

    // Room for the longest line the readout can show, as multiples of the text size.
    m_padding = sizes.padding;
    m_lineHeight = m_value.textSize * 1.5f;
    m_labelWidth = m_label.textSize * 4.5f;
    m_valueWidth = m_value.textSize * 13.f;
    m_batch.setSize(
        { m_padding.x * 2.f + m_labelWidth + m_valueWidth,
          m_padding.y * 2.f + m_lineHeight * static_cast<float>(lineCount) }
    );
    m_batch.markDirty();
}

bool Profiler::refresh(Clock::time_point now) {
    if (!isVisible()) {
        return false;
    }

    if (!m_refreshed.has_value()) {
        // The first look: nothing is measured yet.
        static_cast<void>(take(now));
        m_refreshed = now;
        write(m_readout);
    } else if (now - *m_refreshed >= refreshInterval) {
        m_refreshed = now;
        const Readout latest = take(now);
        if (latest.frames > 0) {
            m_readout = latest;
        } else {
            // Nothing was drawn. Keep what the last frames took, and say that it is over.
            m_readout.seconds = latest.seconds;
            m_readout.frames = 0;
            m_readout.skipped = latest.skipped;
            m_readout.framesPerSecond = 0.;
        }

        const auto shown = m_lines;
        write(m_readout);
        if (m_lines != shown) {
            m_batch.markDirty();
        }
    }

    if (!m_batch.isDirty()) {
        return false;
    }
    paint();
    return true;
}

void Profiler::write(const Readout& readout) {
    const auto print = [this](std::size_t line, const char* format, auto... values) {
        std::snprintf(m_lines[line].data(), m_lines[line].size(), format, values...);
    };

    if (readout.frames > 0) {
        print(0, "%.0f per second", readout.framesPerSecond);
    } else {
        print(0, "%s", "idle");
    }
    print(1, "%.3f ms  (max %.3f)", readout.build.average, readout.build.max);
    print(2, "%.3f ms  (max %.3f)", readout.submit.average, readout.submit.max);
    print(3, "%zu calls, %zu triangles", readout.lastFrame.drawCalls, readout.lastFrame.triangles);
    print(4, "%.1f panels, %.1f texts", readout.panelsRebuilt, readout.textsBuilt);
}

void Profiler::paint() {
    const auto layers = m_batch.rebuild();
    const sf::Vector2f size = m_batch.size();
    Painter painter(layers.frame, { 0.f, 0.f }, size);

    painter.box(FloatRect({ 0.f, 0.f }, size), m_background);
    for (std::size_t i = 0; i < lineCount; ++i) {
        const float top = m_padding.y + m_lineHeight * static_cast<float>(i);
        painter.text(FloatRect(m_padding.x, top, m_labelWidth, m_lineHeight), labels[i], m_label);
        painter.text(
            FloatRect(m_padding.x + m_labelWidth, top, m_valueWidth, m_lineHeight), textOf(m_lines[i]), m_value
        );
    }
}

} // namespace atpl::render
