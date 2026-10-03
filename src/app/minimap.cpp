#include "atpl/app/minimap.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace atpl {

namespace {

/// A drag shorter than this, in pixels, is a click.
constexpr float clickDistance = 3.f;

} // namespace

Minimap::Minimap(std::string viewName, Camera& target, FloatRect world, MinimapOptions options) :
    m_viewName(std::move(viewName)),
    m_target(&target),
    m_world(world),
    m_options(options),
    m_camera(m_viewName, CameraOptions{ .minZoom = 1.0e-6f, .maxZoom = 1.0e6f }) {
    m_camera.show(m_world.position(), m_world.size());
}

bool Minimap::handle(const Event& event) {
    if (const auto* press = event.getIf<PointerPressed>()) {
        if (press->button != m_options.button || !press->pointer.isIn(m_viewName)) {
            return false;
        }
        m_pressed = true;
        m_pointer = press->pointer.inView;
        m_pressedAt = m_pointer;
        if (m_options.mode == MinimapMode::Select) {
            m_from = toWorld(m_pointer);
            return true;
        }
        centreOn(toWorld(m_pointer));
        return true;
    }
    if (const auto* move = event.getIf<PointerMoved>()) {
        if (!m_pressed) {
            return false;
        }
        // Followed by its moves, so that a drag goes on where the pointer leaves the minimap.
        m_pointer += move->delta;
        if (m_options.mode == MinimapMode::Pan) {
            centreOn(toWorld(m_pointer));
        }
        return true;
    }
    if (const auto* release = event.getIf<PointerReleased>()) {
        if (!m_pressed || release->button != m_options.button) {
            return false;
        }
        m_pressed = false;
        if (m_options.mode == MinimapMode::Select && m_from.has_value()) {
            const sf::Vector2f dragged = m_pointer - m_pressedAt;
            const sf::Vector2f to = toWorld(m_pointer);
            if (std::abs(dragged.x) < clickDistance && std::abs(dragged.y) < clickDistance) {
                centreOn(to); // a click: there
            } else {
                const sf::Vector2f corner(std::min(m_from->x, to.x), std::min(m_from->y, to.y));
                const sf::Vector2f size(std::abs(to.x - m_from->x), std::abs(to.y - m_from->y));
                m_target->show(corner, size);
                centreOn(m_target->center());
            }
            m_from.reset();
        }
        return true;
    }
    if (const auto* key = event.getIf<KeyPressed>()) {
        if (!key->isFor(m_viewName)) {
            return false;
        }
        sf::Vector2f direction;
        switch (key->key) {
            case sf::Keyboard::Key::Left:
                direction = { -1.f, 0.f };
                break;
            case sf::Keyboard::Key::Right:
                direction = { 1.f, 0.f };
                break;
            case sf::Keyboard::Key::Up:
                direction = { 0.f, -1.f };
                break;
            case sf::Keyboard::Key::Down:
                direction = { 0.f, 1.f };
                break;
            default:
                return false;
        }
        const sf::Vector2f seen = m_target->visibleArea().size();
        const sf::Vector2f before = m_target->center();
        centreOn(before + sf::Vector2f(direction.x * seen.x, direction.y * seen.y) * m_options.keyStep);
        return m_target->center() != before;
    }
    return false;
}

void Minimap::centreOn(sf::Vector2f worldPosition) {
    m_target->setCenter(
        { std::clamp(worldPosition.x, m_world.left(), m_world.right()),
          std::clamp(worldPosition.y, m_world.top(), m_world.bottom()) }
    );
}

void Minimap::apply(sf::RenderTarget& target, sf::Vector2f viewSize) {
    m_camera.show(m_world.position(), m_world.size());
    m_camera.apply(target, viewSize);
}

void Minimap::drawMarks(sf::RenderTarget& target, sf::Color color) const {
    const float pixel = 1.f / m_camera.zoom();
    const auto outline = [&](const FloatRect& area, sf::Color lineColor) {
        sf::RectangleShape box(area.size());
        box.setPosition(area.position());
        box.setFillColor(sf::Color::Transparent);
        box.setOutlineColor(lineColor);
        box.setOutlineThickness(pixel);
        target.draw(box);
    };
    outline(m_target->visibleArea(), color);
    if (const std::optional<FloatRect> marked = selection()) {
        sf::Color fainter = color;
        fainter.a = static_cast<std::uint8_t>(color.a / 2);
        outline(*marked, fainter);
    }
}

MinimapMode Minimap::mode() const {
    return m_options.mode;
}

void Minimap::setMode(MinimapMode mode) {
    m_options.mode = mode;
    m_from.reset();
}

FloatRect Minimap::world() const {
    return m_world;
}

void Minimap::setWorld(FloatRect world) {
    m_world = world;
    m_camera.show(m_world.position(), m_world.size());
}

sf::Vector2f Minimap::toWorld(sf::Vector2f inView) const {
    return m_camera.toWorld(inView);
}

std::optional<FloatRect> Minimap::selection() const {
    if (!m_from.has_value() || !m_pressed) {
        return std::nullopt;
    }
    const sf::Vector2f to = toWorld(m_pointer);
    return FloatRect(
        { std::min(m_from->x, to.x), std::min(m_from->y, to.y) },
        { std::abs(to.x - m_from->x), std::abs(to.y - m_from->y) }
    );
}

} // namespace atpl
