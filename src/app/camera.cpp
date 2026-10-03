#include "atpl/app/camera.hpp"

#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace atpl {

Camera::Camera(std::string viewName, CameraOptions options) :
    m_viewName(std::move(viewName)),
    m_options(options) {}

bool Camera::handle(const Event& event) {
    if (const auto* press = event.getIf<PointerPressed>()) {
        // A drag starts in the view; it goes on wherever the pointer goes, until the release.
        if (press->button == m_options.dragButton && press->pointer.isIn(m_viewName)) {
            m_dragging = true;
        }
        return false;
    }
    if (const auto* release = event.getIf<PointerReleased>()) {
        if (release->button == m_options.dragButton) {
            m_dragging = false;
        }
        return false;
    }
    if (const auto* move = event.getIf<PointerMoved>()) {
        if (!m_dragging || (move->delta.x == 0.f && move->delta.y == 0.f)) {
            return false;
        }
        m_center -= move->delta / m_zoom; // the world follows the pointer
        return true;
    }
    if (const auto* wheel = event.getIf<Scrolled>()) {
        if (wheel->horizontal || wheel->delta == 0.f || !wheel->pointer.isIn(m_viewName)) {
            return false;
        }
        const float before = m_zoom;
        zoomAround(wheel->pointer.inView, std::pow(m_options.zoomStep, wheel->delta));
        return m_zoom != before;
    }
    return false;
}

void Camera::zoomAround(sf::Vector2f inView, float factor) {
    const sf::Vector2f anchor = toWorld(inView);
    m_zoom = std::clamp(m_zoom * factor, m_options.minZoom, m_options.maxZoom);
    m_center = anchor - (inView - m_viewSize * 0.5f) / m_zoom; // the same world position under the pointer
}

void Camera::apply(sf::RenderTarget& target, sf::Vector2f viewSize) {
    m_viewSize = viewSize;
    if (m_shown.has_value()) {
        show(m_shown->position(), m_shown->size()); // now that the view's size is known
    }
    // The UI has set up where the view is and what of it can be seen; only what is shown in it
    // changes.
    sf::View view = target.getView();
    view.setSize(viewSize / m_zoom);
    view.setCenter(m_center);
    target.setView(view);
}

sf::Vector2f Camera::toWorld(sf::Vector2f inView) const {
    return m_center + (inView - m_viewSize * 0.5f) / m_zoom;
}

sf::Vector2f Camera::center() const {
    return m_center;
}

void Camera::setCenter(sf::Vector2f worldPosition) {
    m_center = worldPosition;
}

float Camera::zoom() const {
    return m_zoom;
}

void Camera::setZoom(float pixelsPerUnit) {
    m_zoom = std::clamp(pixelsPerUnit, m_options.minZoom, m_options.maxZoom);
}

void Camera::show(sf::Vector2f worldTopLeft, sf::Vector2f worldSize) {
    m_center = worldTopLeft + worldSize * 0.5f;
    if (m_viewSize.x <= 0.f || m_viewSize.y <= 0.f || worldSize.x <= 0.f || worldSize.y <= 0.f) {
        m_shown = FloatRect(worldTopLeft, worldSize); // the zoom follows once the view's size is known
        return;
    }
    m_shown.reset();
    setZoom(std::min(m_viewSize.x / worldSize.x, m_viewSize.y / worldSize.y));
}

FloatRect Camera::visibleArea() const {
    const sf::Vector2f size = m_viewSize / m_zoom;
    return { m_center - size * 0.5f, size };
}

} // namespace atpl
