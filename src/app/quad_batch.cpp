#include "atpl/app/quad_batch.hpp"

#include <SFML/Graphics/PrimitiveType.hpp>

#include <cassert>

namespace atpl {

namespace {

/// The corners of a rectangle, top-left, top-right, bottom-right, bottom-left.
std::array<sf::Vector2f, 4> cornersOf(FloatRect rect) {
    return { rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft() };
}

/// The corners of a rectangle of `size` around `centre`, turned by `angle`.
std::array<sf::Vector2f, 4> cornersOf(sf::Vector2f centre, sf::Vector2f size, sf::Angle angle) {
    const sf::Vector2f half = size / 2.f;
    const std::array<sf::Vector2f, 4> offsets{ sf::Vector2f(-half.x, -half.y),
                                               sf::Vector2f(half.x, -half.y),
                                               sf::Vector2f(half.x, half.y),
                                               sf::Vector2f(-half.x, half.y) };
    std::array<sf::Vector2f, 4> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        corners[i] = centre + offsets[i].rotatedBy(angle);
    }
    return corners;
}

std::array<sf::Color, 4> allOf(sf::Color color) {
    return { color, color, color, color };
}

// The two triangles of a quad: corners 0, 1, 2 and 0, 2, 3.
constexpr std::array<std::size_t, QuadBatch::verticesPerQuad> cornerOfVertex{ 0, 1, 2, 0, 2, 3 };

} // namespace

QuadBatch::QuadBatch(const sf::Texture* texture) :
    m_texture(texture) {}

void QuadBatch::setTexture(const sf::Texture* texture) {
    m_texture = texture;
}

void QuadBatch::clear() {
    m_vertices.clear();
}

void QuadBatch::reserve(std::size_t quads) {
    m_vertices.reserve(quads * verticesPerQuad);
}

void QuadBatch::resize(std::size_t quads) {
    m_vertices.resize(quads * verticesPerQuad, sf::Vertex{ {}, sf::Color::Transparent, {} });
}

std::size_t QuadBatch::add(FloatRect rect, sf::Color color) {
    return add(rect, wholeTexture(), color);
}

std::size_t QuadBatch::add(FloatRect rect, FloatRect textureRect, sf::Color color) {
    const std::size_t index = size();
    resize(index + 1);
    set(index, rect, textureRect, color);
    return index;
}

std::size_t QuadBatch::add(sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, sf::Color color) {
    return add(centre, size, angle, wholeTexture(), color);
}

std::size_t
QuadBatch::add(sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, FloatRect textureRect, sf::Color color) {
    const std::size_t index = this->size();
    resize(index + 1);
    set(index, centre, size, angle, textureRect, color);
    return index;
}

void QuadBatch::set(std::size_t index, FloatRect rect, sf::Color color) {
    set(index, rect, wholeTexture(), color);
}

void QuadBatch::set(std::size_t index, FloatRect rect, FloatRect textureRect, sf::Color color) {
    set(index, cornersOf(rect), allOf(color), cornersOf(textureRect));
}

void QuadBatch::set(std::size_t index, sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, sf::Color color) {
    set(index, centre, size, angle, wholeTexture(), color);
}

void QuadBatch::set(
    std::size_t index, sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, FloatRect textureRect, sf::Color color
) {
    set(index, cornersOf(centre, size, angle), allOf(color), cornersOf(textureRect));
}

void QuadBatch::set(
    std::size_t index,
    const std::array<sf::Vector2f, 4>& corners,
    const std::array<sf::Color, 4>& colors,
    const std::array<sf::Vector2f, 4>& texturePoints
) {
    assert(index < size());
    sf::Vertex* quad = &m_vertices[index * verticesPerQuad];
    for (std::size_t i = 0; i < verticesPerQuad; ++i) {
        const std::size_t corner = cornerOfVertex[i];
        quad[i] = sf::Vertex{ corners[corner], colors[corner], texturePoints[corner] };
    }
}

void QuadBatch::setColor(std::size_t index, sf::Color color) {
    setColors(index, allOf(color));
}

void QuadBatch::setColors(std::size_t index, const std::array<sf::Color, 4>& colors) {
    assert(index < size());
    sf::Vertex* quad = &m_vertices[index * verticesPerQuad];
    for (std::size_t i = 0; i < verticesPerQuad; ++i) {
        quad[i].color = colors[cornerOfVertex[i]];
    }
}

void QuadBatch::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (m_vertices.empty()) {
        return;
    }
    states.texture = m_texture;
    target.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles, states);
}

FloatRect QuadBatch::wholeTexture() const {
    if (m_texture == nullptr) {
        return {};
    }
    const sf::Vector2u size = m_texture->getSize();
    return FloatRect(0.f, 0.f, static_cast<float>(size.x), static_cast<float>(size.y));
}

} // namespace atpl
