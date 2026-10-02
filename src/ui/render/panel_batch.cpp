#include "ui/render/panel_batch.hpp"

namespace atpl::render {

PanelBatch::Layers PanelBatch::rebuild() {
    m_frame.clear();
    m_content.clear();
    m_dirty = false;
    m_changed = true;
    ++m_rebuildCount;
    return { m_frame, m_content };
}

bool PanelBatch::takeChanged() {
    const bool changed = m_changed;
    m_changed = false;
    return changed;
}

void PanelBatch::setPosition(sf::Vector2f position) {
    if (position != m_position) {
        m_position = position;
        m_changed = true;
    }
}

void PanelBatch::setSize(sf::Vector2f size) {
    if (size != m_size) {
        m_size = size;
        m_dirty = true;
    }
}

void PanelBatch::setContentClip(std::optional<FloatRect> clip) {
    if (clip != m_contentClip) {
        m_contentClip = clip;
        m_changed = true;
    }
}

void PanelBatch::setScroll(float offset) {
    if (offset != m_scroll) {
        m_scroll = offset;
        m_changed = true;
    }
}

void PanelBatch::setVisible(bool visible) {
    if (visible != m_visible) {
        m_visible = visible;
        m_changed = true;
    }
}

sf::Transform PanelBatch::frameTransform() const {
    sf::Transform transform;
    transform.translate(m_position);
    return transform;
}

sf::Transform PanelBatch::contentTransform() const {
    sf::Transform transform;
    transform.translate({ m_position.x, m_position.y - m_scroll });
    return transform;
}

std::optional<FloatRect> PanelBatch::clipInWindow() const {
    if (!m_contentClip.has_value()) {
        return std::nullopt;
    }
    return FloatRect(m_contentClip->position() + m_position, m_contentClip->size());
}

} // namespace atpl::render
