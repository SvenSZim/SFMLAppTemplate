#include "ui/render/renderer.hpp"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>

namespace atpl::render {

sf::FloatRect scissorFor(const FloatRect& clip, sf::Vector2u targetSize) {
    if (targetSize.x == 0 || targetSize.y == 0) {
        return { { 0.f, 0.f }, { 0.f, 0.f } };
    }
    const auto width = static_cast<float>(targetSize.x);
    const auto height = static_cast<float>(targetSize.y);

    const float left = std::clamp(clip.left() / width, 0.f, 1.f);
    const float top = std::clamp(clip.top() / height, 0.f, 1.f);
    const float right = std::clamp(clip.right() / width, 0.f, 1.f);
    const float bottom = std::clamp(clip.bottom() / height, 0.f, 1.f);
    return { { left, top }, { std::max(right - left, 0.f), std::max(bottom - top, 0.f) } };
}

FrameStats
Renderer::draw(sf::RenderTarget& target, std::span<const PanelBatch* const> panels, const PanelBatch* overlay) {
    FrameStats stats;
    for (const PanelBatch* batch : panels) {
        if (batch != nullptr) {
            drawBatch(target, *batch, stats);
        }
    }
    if (overlay != nullptr) {
        drawBatch(target, *overlay, stats);
    }
    return stats;
}

void Renderer::drawBatch(sf::RenderTarget& target, const PanelBatch& batch, FrameStats& stats) {
    if (!batch.isVisible()) {
        return;
    }
    ++stats.panelsDrawn;

    drawLayer(target, batch.frame(), batch.frameTransform(), stats);

    if (batch.content().empty()) {
        return;
    }
    const auto clip = batch.clipInWindow();
    if (!clip.has_value()) {
        drawLayer(target, batch.content(), batch.contentTransform(), stats);
        return;
    }

    // Clipped content: narrow what the target lets through, draw, and put it back.
    const sf::View unclipped = target.getView();
    sf::View clipped = unclipped;
    clipped.setScissor(scissorFor(*clip, target.getSize()));
    target.setView(clipped);
    drawLayer(target, batch.content(), batch.contentTransform(), stats);
    target.setView(unclipped);
}

void Renderer::drawLayer(
    sf::RenderTarget& target, const DrawList& layer, const sf::Transform& transform, FrameStats& stats
) {
    const sf::RenderStates states(transform);

    const VertexList& shapes = layer.shapes();
    if (!shapes.empty()) {
        target.draw(shapes.data(), shapes.size(), sf::PrimitiveType::Triangles, states);
        ++stats.drawCalls;
        stats.triangles += shapes.size() / 3;
    }

    if (m_textRenderer != nullptr && !layer.texts().empty()) {
        stats.drawCalls += m_textRenderer->draw(target, states, layer);
    }
}

} // namespace atpl::render
