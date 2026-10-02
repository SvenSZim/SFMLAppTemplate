#pragma once

#include "atpl/ui/theme.hpp"

#include "ui/render/frame_stats.hpp"
#include "ui/render/panel_batch.hpp"

#include <SFML/System/Vector2.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

namespace atpl::render {

/// Measures what the UI costs, and can show it on screen (D6).
///
/// Three sections of a frame are timed:
///
///   Build    painting the panels that changed into their batches
///   Submit   clearing the window and handing the batches to the graphics card
///   Show     putting the finished frame on screen; with vsync on, this waits for the display
///
/// Build and submit are the UI's own cost. Show is listed apart because it is mostly waiting.
///
/// Whoever does the work reports it: the code that paints panels wraps that in
/// `measure(Section::Build)`, the renderer reports the rest. Measuring costs two readings of the
/// clock per section and allocates nothing.
///
/// The readout is a small panel of its own, drawn on top of everything. It is refreshed a few
/// times per second, not every frame, and only when its text would change: an application that is
/// idle stays idle with the readout on. What the readout itself costs is not part of the numbers.
class Profiler {
public:
    using Clock = std::chrono::steady_clock;

    // The readout's parts, so that a theme can style it like anything else.
    static constexpr Kind kind{ "profiler" };
    static constexpr Part Background{ kind, "background", Role::Surface };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part Value{ kind, "value", Role::Text };

    enum class Section { Build, Submit, Show };

    /// Average and largest time of a section per frame, in milliseconds.
    struct Timing {
        double average = 0.;
        double max = 0.;
    };

    /// What was measured over a stretch of time.
    struct Readout {
        double seconds = 0.;     ///< How long the stretch was.
        std::size_t frames = 0;  ///< Frames drawn in it.
        std::size_t skipped = 0; ///< Frames that were not needed, and so not drawn.
        double framesPerSecond = 0.;

        Timing build;
        Timing submit;
        Timing show;

        FrameStats lastFrame;      ///< Draw calls and triangles of the last frame drawn.
        double panelsRebuilt = 0.; ///< Panels painted anew, per frame.
        double textsBuilt = 0.;    ///< Text objects built, per frame.
    };

    Profiler();

    // ----- Measuring -----

    /// Adds time to a section of the frame being made.
    void record(Section section, Clock::duration time);

    /// Times a section from here to the end of the scope. A null profiler measures nothing.
    class Scope {
    public:
        Scope(Profiler* profiler, Section section) :
            m_profiler(profiler),
            m_section(section),
            m_start(profiler != nullptr ? Clock::now() : Clock::time_point{}) {}
        ~Scope() {
            if (m_profiler != nullptr) {
                m_profiler->record(m_section, Clock::now() - m_start);
            }
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        Profiler* m_profiler;
        Section m_section;
        Clock::time_point m_start;
    };
    [[nodiscard]] Scope measure(Section section) { return { this, section }; }

    /// A frame was drawn: what was recorded since the last one belongs to it.
    void frameDrawn(const FrameStats& stats, std::size_t panelsRebuilt, std::size_t textsBuilt);

    /// A frame was not needed.
    void frameSkipped();

    /// Forgets what was recorded for the frame being made. The renderer does this for a frame
    /// that was drawn only to refresh the readout.
    void dropFrame();

    /// What was measured since the last call, and starts a new stretch at `now`.
    Readout take(Clock::time_point now);

    // ----- The readout on screen -----

    /// Off by default. While it is off, nothing is refreshed or drawn.
    void setVisible(bool visible);
    [[nodiscard]] bool isVisible() const { return m_batch.isVisible(); }

    /// Takes the readout's colours, text sizes and spacing from the theme. Until this is called
    /// the readout has no look and shows nothing.
    void setLook(const Theme& theme, PanelColors colors = {});

    /// The readout's top-left corner in the window.
    void setPosition(sf::Vector2f position) { m_batch.setPosition(position); }

    /// Refreshes the readout if it is visible and due: every `refreshInterval`, and only if its
    /// text is different from what is shown. Returns whether it was painted anew.
    bool refresh(Clock::time_point now);

    /// What the readout shows; empty lines before the first refresh.
    [[nodiscard]] const Readout& readout() const { return m_readout; }

    /// The readout as a batch, for the renderer to draw last.
    [[nodiscard]] PanelBatch& batch() { return m_batch; }
    [[nodiscard]] const PanelBatch& batch() const { return m_batch; }

    /// Live values are refreshed at 5 Hz (plan 5.2, level 3).
    static constexpr std::chrono::milliseconds refreshInterval{ 200 };

    static constexpr std::size_t lineCount = 5;

private:
    struct Sum {
        double total = 0.; // milliseconds over all frames of the stretch
        double max = 0.;
        double pending = 0.; // of the frame being made

        void closeFrame();
        [[nodiscard]] Timing over(std::size_t frames) const;
    };
    using Line = std::array<char, 48>;

    [[nodiscard]] Sum& sum(Section section);
    void write(const Readout& readout);
    void paint();

    // The stretch being measured.
    std::optional<Clock::time_point> m_start;
    std::size_t m_frames = 0;
    std::size_t m_skipped = 0;
    Sum m_build;
    Sum m_submit;
    Sum m_show;
    FrameStats m_lastFrame;
    std::size_t m_panelsRebuilt = 0;
    std::size_t m_textsBuilt = 0;

    // The readout.
    Readout m_readout;
    std::array<Line, lineCount> m_lines{};
    std::optional<Clock::time_point> m_refreshed;
    PanelBatch m_batch;
    PartStyle m_background;
    PartStyle m_label;
    PartStyle m_value;
    float m_padding = 0.f;
    float m_lineHeight = 0.f;
    float m_labelWidth = 0.f;
    float m_valueWidth = 0.f;
};

} // namespace atpl::render
