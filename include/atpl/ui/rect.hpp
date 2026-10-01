#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <type_traits>

namespace atpl {

/// An axis-aligned rectangle: the position of its top-left corner and its size.
///
/// A plain value. Adding, subtracting and scaling work per component, which is what lets a
/// rectangle be animated with `Interpolated<Rect<T>>`.
template <typename T>
class Rect {
    static_assert(std::is_arithmetic_v<T>, "Rect needs a number type");

public:
    constexpr Rect() = default;

    constexpr Rect(T left, T top, T width, T height) :
        m_left(left),
        m_top(top),
        m_width(width),
        m_height(height) {}

    constexpr Rect(sf::Vector2<T> position, sf::Vector2<T> size) :
        Rect(position.x, position.y, size.x, size.y) {}

    constexpr Rect(const sf::Rect<T>& rect) :
        Rect(rect.position, rect.size) {}

    // Edges

    [[nodiscard]] constexpr T left() const { return m_left; }
    [[nodiscard]] constexpr T top() const { return m_top; }
    [[nodiscard]] constexpr T right() const { return m_left + m_width; }
    [[nodiscard]] constexpr T bottom() const { return m_top + m_height; }
    [[nodiscard]] constexpr T width() const { return m_width; }
    [[nodiscard]] constexpr T height() const { return m_height; }

    constexpr void setLeft(T left) { m_left = left; }
    constexpr void setTop(T top) { m_top = top; }
    constexpr void setWidth(T width) { m_width = width; }
    constexpr void setHeight(T height) { m_height = height; }

    // Position and size

    [[nodiscard]] constexpr sf::Vector2<T> position() const { return {m_left, m_top}; }
    [[nodiscard]] constexpr sf::Vector2<T> size() const { return {m_width, m_height}; }

    constexpr void setPosition(sf::Vector2<T> position) {
        m_left = position.x;
        m_top = position.y;
    }

    constexpr void setSize(sf::Vector2<T> size) {
        m_width = size.x;
        m_height = size.y;
    }

    // Points

    [[nodiscard]] constexpr sf::Vector2<T> topLeft() const { return {left(), top()}; }
    [[nodiscard]] constexpr sf::Vector2<T> topRight() const { return {right(), top()}; }
    [[nodiscard]] constexpr sf::Vector2<T> bottomLeft() const { return {left(), bottom()}; }
    [[nodiscard]] constexpr sf::Vector2<T> bottomRight() const { return {right(), bottom()}; }
    [[nodiscard]] constexpr sf::Vector2<T> center() const { return {m_left + m_width / T{2}, m_top + m_height / T{2}}; }

    /// Whether the point lies inside. The left and top edges count as inside, the right and
    /// bottom edges do not, so two rectangles that share an edge never both contain a point.
    [[nodiscard]] constexpr bool contains(sf::Vector2<T> point) const {
        return point.x >= left() && point.x < right() && point.y >= top() && point.y < bottom();
    }

    /// The rectangle moved inwards by `delta` on every side (outwards if negative).
    [[nodiscard]] constexpr Rect inset(T delta) const {
        return {m_left + delta, m_top + delta, m_width - delta * T{2}, m_height - delta * T{2}};
    }

    [[nodiscard]] constexpr sf::Rect<T> toSFMLRect() const { return {position(), size()}; }

    // Arithmetic per component

    [[nodiscard]] constexpr Rect operator+(const Rect& other) const {
        return {m_left + other.m_left, m_top + other.m_top, m_width + other.m_width, m_height + other.m_height};
    }

    [[nodiscard]] constexpr Rect operator-(const Rect& other) const {
        return {m_left - other.m_left, m_top - other.m_top, m_width - other.m_width, m_height - other.m_height};
    }

    [[nodiscard]] constexpr Rect operator*(T scalar) const {
        return {m_left * scalar, m_top * scalar, m_width * scalar, m_height * scalar};
    }

    /// Scales every component by a ratio; the form `Interpolated` uses.
    [[nodiscard]] friend constexpr Rect operator*(float ratio, const Rect& rect) {
        return {
            static_cast<T>(ratio * static_cast<float>(rect.m_left)),
            static_cast<T>(ratio * static_cast<float>(rect.m_top)),
            static_cast<T>(ratio * static_cast<float>(rect.m_width)),
            static_cast<T>(ratio * static_cast<float>(rect.m_height)),
        };
    }

    [[nodiscard]] friend constexpr bool operator==(const Rect&, const Rect&) = default;

private:
    T m_left{};
    T m_top{};
    T m_width{};
    T m_height{};
};

using FloatRect = Rect<float>;
using IntRect = Rect<int>;

} // namespace atpl
