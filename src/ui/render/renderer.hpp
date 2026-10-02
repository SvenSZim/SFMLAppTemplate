#pragma once

#include "ui/render/panel_batch.hpp"
#include "ui/render/text_renderer.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <span>

namespace atpl::render {

/// What drawing one frame took.
struct FrameStats {
    std::size_t drawCalls = 0;   ///< Calls that sent geometry to the graphics card.
    std::size_t triangles = 0;   ///< Triangles of shapes in those calls. Text is not counted.
    std::size_t panelsDrawn = 0; ///< Batches that were visible, the overlay included.
};

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

    /// Draws `panels` in order and then `overlay`, if there is one.
    FrameStats
    draw(sf::RenderTarget& target, std::span<const PanelBatch* const> panels, const PanelBatch* overlay = nullptr);

private:
    void drawBatch(sf::RenderTarget& target, const PanelBatch& batch, FrameStats& stats);
    void drawLayer(sf::RenderTarget& target, const DrawList& layer, const sf::Transform& transform, FrameStats& stats);

    TextRenderer* m_textRenderer;
};

/// The part of a target, as factors from 0 to 1, that a clip rectangle given in pixels covers:
/// the form a view's scissor takes. A rectangle that reaches beyond the target is cut to it.
[[nodiscard]] sf::FloatRect scissorFor(const FloatRect& clip, sf::Vector2u targetSize);

} // namespace atpl::render
