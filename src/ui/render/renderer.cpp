#include "ui/render/renderer.hpp"

#include "ui/render/profiler.hpp"

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
    if (m_viewPainter) {
        m_viewPainter(target, std::nullopt);
    }
    for (std::size_t i = 0; i < panels.size(); ++i) {
        if (panels[i] != nullptr) {
            drawBatch(target, *panels[i], stats);
            if (m_viewPainter && panels[i]->isVisible()) {
                m_viewPainter(target, i);
            }
        }
    }
    if (overlay != nullptr) {
        drawBatch(target, *overlay, stats);
    }
    return stats;
}

std::optional<FrameStats> Renderer::present(
    sf::RenderWindow& window,
    sf::Color background,
    frame::RedrawFlag& flag,
    std::span<PanelBatch* const> panels,
    PanelBatch* overlay
) {
    // Ask every batch, not just until the first that changed: each mark has to be cleared.
    bool needed = flag.take();
    for (PanelBatch* batch : panels) {
        if (batch != nullptr && batch->takeChanged()) {
            needed = true;
        }
    }
    if (overlay != nullptr && overlay->takeChanged()) {
        needed = true;
    }

    // The readout comes last: whether it alone is the reason for this frame matters below.
    const bool neededByUI = needed;
    if (m_profiler != nullptr) {
        m_profiler->refresh(Profiler::Clock::now());
        // Also when the readout was just switched off: it has to disappear from the screen.
        if (m_profiler->batch().takeChanged()) {
            needed = true;
        }
    }

    if (!needed) {
        ++m_framesSkipped;
        if (m_profiler != nullptr) {
            m_profiler->frameSkipped();
        }
        return std::nullopt;
    }

    FrameStats stats;
    std::size_t rebuilds = 0;
    const std::size_t textsBefore = m_textRenderer != nullptr ? m_textRenderer->buildCount() : 0;
    {
        const Profiler::Scope submit(m_profiler, Profiler::Section::Submit);
        window.clear(background);
        if (m_viewPainter) {
            m_viewPainter(window, std::nullopt); // the view behind the panels
        }
        for (std::size_t i = 0; i < panels.size(); ++i) {
            if (panels[i] != nullptr) {
                drawBatch(window, *panels[i], stats);
                rebuilds += panels[i]->rebuildCount();
                if (m_viewPainter && panels[i]->isVisible()) {
                    m_viewPainter(window, i); // the views inside it, above it and below the next
                }
            }
        }
        if (overlay != nullptr) {
            drawBatch(window, *overlay, stats);
            rebuilds += overlay->rebuildCount();
        }
    }
    const std::size_t textsBuilt = m_textRenderer != nullptr ? m_textRenderer->buildCount() - textsBefore : 0;

    // The readout is drawn outside of what is measured and counted: it shows what the UI costs,
    // not what it costs itself.
    if (m_profiler != nullptr) {
        FrameStats ownStats;
        drawBatch(window, m_profiler->batch(), ownStats);
    }
    {
        const Profiler::Scope show(m_profiler, Profiler::Section::Show);
        window.display();
    }
    ++m_framesDrawn;

    if (m_profiler != nullptr) {
        // Rebuilds are counted by the batches; the difference to the last frame is this frame's.
        // A different list of panels can make the sum smaller: then nothing is reported.
        const std::size_t panelsRebuilt = rebuilds > m_rebuildsSeen ? rebuilds - m_rebuildsSeen : 0;
        if (neededByUI) {
            m_profiler->frameDrawn(stats, panelsRebuilt, textsBuilt);
        } else {
            m_profiler->dropFrame();
        }
    }
    m_rebuildsSeen = rebuilds;
    return stats;
}

void Renderer::drawBatch(sf::RenderTarget& target, const PanelBatch& batch, FrameStats& stats) {
    if (!batch.isVisible()) {
        return;
    }
    ++stats.panelsDrawn;

    drawLayer(target, batch.frame(), batch.frameTransform(), stats);

    if (!batch.content().empty()) {
        const auto clip = batch.clipInWindow();
        if (!clip.has_value()) {
            drawLayer(target, batch.content(), batch.contentTransform(), stats);
        } else {
            // Clipped content: narrow what the target lets through, draw, and put it back.
            const sf::View unclipped = target.getView();
            sf::View clipped = unclipped;
            clipped.setScissor(scissorFor(*clip, target.getSize()));
            target.setView(clipped);
            drawLayer(target, batch.content(), batch.contentTransform(), stats);
            target.setView(unclipped);
        }
    }

    if (!batch.scrollbar().empty()) {
        drawLayer(target, batch.scrollbar(), batch.scrollbarTransform(), stats); // above the content
    }
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
        const std::size_t calls = m_textRenderer->draw(target, states, layer);
        stats.drawCalls += calls;
        stats.textCalls += calls;
    }
}

} // namespace atpl::render
