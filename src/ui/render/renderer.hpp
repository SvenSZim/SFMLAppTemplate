#pragma once

#include "ui/frame_loop.hpp"
#include "ui/render/frame_stats.hpp"
#include "ui/render/panel_batch.hpp"
#include "ui/render/text_renderer.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace atpl::render {

class Profiler;

/// Draws batches in a fixed order.
///
/// The order is the order of the list it is given, first to last, followed by the overlay. There
/// is no sorting and no lookup: what is listed later is drawn on top.
///
/// Per batch: the frame's shapes, the frame's text, then the content's shapes and text, clipped
/// if the batch says so. That is at most two calls for shapes per panel, however many widgets it has.
///
/// The target's view must map one unit to one pixel with (0, 0) at the top-left corner.
class Renderer {
public:
    /// Without a text renderer, text is not drawn.
    explicit Renderer(TextRenderer* textRenderer = nullptr) :
        m_textRenderer(textRenderer) {}

    void setTextRenderer(TextRenderer* textRenderer) { m_textRenderer = textRenderer; }

    /// With a profiler, `present` reports what each frame took to it, and draws its readout on
    /// top of everything while the readout is visible.
    void setProfiler(Profiler* profiler) { m_profiler = profiler; }

    /// Draws `panels` in order and then `overlay`, if there is one.
    FrameStats
    draw(sf::RenderTarget& target, std::span<const PanelBatch* const> panels, const PanelBatch* overlay = nullptr);

    /// Shows a new frame in the window, if one is needed (cache level 1).
    ///
    /// A frame is needed if `flag` asks for one, or if a batch changed since the last frame. Then
    /// the window is cleared to `background`, the batches are drawn and the result is shown.
    /// Otherwise nothing is done at all: no clearing, no drawing, no showing; the window keeps
    /// what it has, and the result is empty.
    ///
    /// A visible profiler readout is refreshed here and drawn last. A frame drawn only because
    /// the readout's text changed is not counted in the profiler's numbers.
    std::optional<FrameStats> present(
        sf::RenderWindow& window,
        sf::Color background,
        frame::RedrawFlag& flag,
        std::span<PanelBatch* const> panels,
        PanelBatch* overlay = nullptr
    );

    /// Frames shown and frames skipped by `present` so far. For tests and for the profiler.
    [[nodiscard]] std::size_t framesDrawn() const { return m_framesDrawn; }
    [[nodiscard]] std::size_t framesSkipped() const { return m_framesSkipped; }

private:
    void drawBatch(sf::RenderTarget& target, const PanelBatch& batch, FrameStats& stats);
    void drawLayer(sf::RenderTarget& target, const DrawList& layer, const sf::Transform& transform, FrameStats& stats);

    TextRenderer* m_textRenderer;
    Profiler* m_profiler = nullptr;
    std::size_t m_rebuildsSeen = 0;
    std::size_t m_framesDrawn = 0;
    std::size_t m_framesSkipped = 0;
};

/// The part of a target, as factors from 0 to 1, that a clip rectangle given in pixels covers:
/// the form a view's scissor takes. A rectangle that reaches beyond the target is cut to it.
[[nodiscard]] sf::FloatRect scissorFor(const FloatRect& clip, sf::Vector2u targetSize);

} // namespace atpl::render
