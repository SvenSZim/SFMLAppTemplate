#include "ui/render/shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace atpl::render {

namespace {

constexpr int maxCornerSegments = 32;
constexpr float maxCurveError = 0.25f; // pixels between a straight piece and the true circle

/// Directions along a quarter circle, from angle 0 to 90 degrees, for every segment count.
/// Computed once; a corner of any radius is these directions scaled.
class CornerTables {
public:
    CornerTables() {
        std::size_t offset = 0;
        for (int segments = 0; segments <= maxCornerSegments; ++segments) {
            m_offsets[static_cast<std::size_t>(segments)] = offset;
            for (int k = 0; k <= segments; ++k) {
                const float angle = segments == 0 ? 0.f
                                                  : static_cast<float>(k) / static_cast<float>(segments) *
                                                        std::numbers::pi_v<float> * 0.5f;
                m_directions[offset++] = { std::cos(angle), std::sin(angle) };
            }
        }
    }

    /// The direction at step `k` of `segments`, as (cos, sin).
    [[nodiscard]] sf::Vector2f direction(int segments, int k) const {
        return m_directions[m_offsets[static_cast<std::size_t>(segments)] + static_cast<std::size_t>(k)];
    }

private:
    // 1 + 2 + ... + (maxCornerSegments + 1) directions in total.
    static constexpr std::size_t total = (maxCornerSegments + 1) * (maxCornerSegments + 2) / 2;

    std::array<sf::Vector2f, total> m_directions{};
    std::array<std::size_t, maxCornerSegments + 1> m_offsets{};
};

const CornerTables& cornerTables() {
    static const CornerTables tables;
    return tables;
}

/// A rectangle with rounded corners, walked point by point around its edge.
struct Outline {
    FloatRect rect;
    float radius = 0.f;
    int segments = 0; ///< Straight pieces per corner. The same for outlines that are joined into a ring.

    [[nodiscard]] int pointCount() const { return 4 * (segments + 1); }

    /// Point `index` of the edge, clockwise, starting where the top-left corner begins.
    [[nodiscard]] sf::Vector2f point(int index) const {
        const int corner = index / (segments + 1);
        const sf::Vector2f d = cornerTables().direction(segments, index % (segments + 1));

        switch (corner) {
            case 0: // top-left: from pointing left to pointing up
                return { rect.left() + radius - d.x * radius, rect.top() + radius - d.y * radius };
            case 1: // top-right: from up to right
                return { rect.right() - radius + d.y * radius, rect.top() + radius - d.x * radius };
            case 2: // bottom-right: from right to down
                return { rect.right() - radius + d.x * radius, rect.bottom() - radius + d.y * radius };
            default: // bottom-left: from down to left
                return { rect.left() + radius - d.y * radius, rect.bottom() - radius + d.x * radius };
        }
    }
};

[[nodiscard]] float halfSmallerSide(const FloatRect& rect) {
    return std::min(rect.width(), rect.height()) * 0.5f;
}

/// An outline for `rect` with the radius limited to what fits.
[[nodiscard]] Outline outlineOf(const FloatRect& rect, float radius, int segments) {
    return { rect, std::clamp(radius, 0.f, std::max(halfSmallerSide(rect), 0.f)), segments };
}

[[nodiscard]] sf::Color withAlpha(sf::Color color, float factor) {
    color.a = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.a) * std::clamp(factor, 0.f, 1.f)));
    return color;
}

void appendTriangle(VertexList& out, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color color) {
    out.push_back({ a, color });
    out.push_back({ b, color });
    out.push_back({ c, color });
}

/// The area inside an outline.
void appendFill(VertexList& out, const Outline& outline, sf::Color color) {
    const FloatRect& r = outline.rect;

    if (outline.segments == 0) { // sharp corners: two triangles
        appendTriangle(out, r.topLeft(), r.topRight(), r.bottomRight(), color);
        appendTriangle(out, r.topLeft(), r.bottomRight(), r.bottomLeft(), color);
        return;
    }

    // A fan around the centre.
    const sf::Vector2f center = r.center();
    const int count = outline.pointCount();
    sf::Vector2f previous = outline.point(count - 1);
    for (int i = 0; i < count; ++i) {
        const sf::Vector2f current = outline.point(i);
        appendTriangle(out, center, previous, current, color);
        previous = current;
    }
}

/// The band between two outlines with the same number of points, with a colour for each edge.
void appendRing(
    VertexList& out, const Outline& outer, sf::Color outerColor, const Outline& inner, sf::Color innerColor
) {
    const int count = outer.pointCount();
    sf::Vector2f previousOuter = outer.point(count - 1);
    sf::Vector2f previousInner = inner.point(count - 1);

    for (int i = 0; i < count; ++i) {
        const sf::Vector2f currentOuter = outer.point(i);
        const sf::Vector2f currentInner = inner.point(i);

        out.push_back({ previousOuter, outerColor });
        out.push_back({ currentOuter, outerColor });
        out.push_back({ currentInner, innerColor });

        out.push_back({ previousOuter, outerColor });
        out.push_back({ currentInner, innerColor });
        out.push_back({ previousInner, innerColor });

        previousOuter = currentOuter;
        previousInner = currentInner;
    }
}

/// A soft shadow: solid well inside the box, fading to nothing `size` pixels outside it.
void appendShadow(VertexList& out, const FloatRect& rect, float radius, const Shadow& shadow) {
    const FloatRect base(rect.position() + shadow.offset, rect.size());
    const float size = shadow.size;
    const float inwards = std::min(size, halfSmallerSide(base));
    const int segments = cornerSegments(radius + size);

    const Outline core = outlineOf(base.inset(inwards), radius - inwards, segments);
    const Outline edge = outlineOf(base, radius, segments);
    const Outline rim = outlineOf(base.inset(-size), radius + size, segments);

    // Not a straight fade: most of the darkness is gone at the box's edge, which reads as soft.
    constexpr float alphaAtEdge = 0.4f;
    const sf::Color solid = shadow.color;
    const sf::Color atEdge = withAlpha(shadow.color, alphaAtEdge);
    const sf::Color clear = withAlpha(shadow.color, 0.f);

    appendFill(out, core, solid);
    appendRing(out, edge, atEdge, core, solid);
    appendRing(out, rim, clear, edge, atEdge);
}

/// The sideways offset that gives a line its thickness: perpendicular to `direction`, `halfWidth` long.
[[nodiscard]] sf::Vector2f sideways(sf::Vector2f direction, float halfWidth) {
    const float length = std::hypot(direction.x, direction.y);
    if (length <= 0.f) {
        return { 0.f, 0.f };
    }
    return { -direction.y / length * halfWidth, direction.x / length * halfWidth };
}

void appendQuad(VertexList& out, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d, sf::Color color) {
    appendTriangle(out, a, b, c, color);
    appendTriangle(out, a, c, d, color);
}

[[nodiscard]] bool drawsLines(const PartStyle& style) {
    return style.shown && style.thickness > 0.f && style.color.a > 0;
}

} // namespace

int cornerSegments(float radius) {
    if (radius <= 0.f) {
        return 0;
    }
    if (radius <= maxCurveError) {
        return 1;
    }
    // A straight piece spanning the angle `step` is at most radius * (1 - cos(step / 2)) away
    // from the circle. Solve for the largest step that stays within the allowed error.
    const float step = 2.f * std::acos(1.f - maxCurveError / radius);
    const int segments = static_cast<int>(std::ceil(std::numbers::pi_v<float> * 0.5f / step));
    return std::clamp(segments, 1, maxCornerSegments);
}

void appendBox(VertexList& out, const FloatRect& rect, const PartStyle& style) {
    if (!style.shown || rect.width() <= 0.f || rect.height() <= 0.f) {
        return;
    }

    const float radius = std::clamp(style.radius, 0.f, halfSmallerSide(rect));
    const int segments = cornerSegments(radius);

    if (style.shadow.size > 0.f && style.shadow.color.a > 0) {
        appendShadow(out, rect, radius, style.shadow);
    }

    const Outline outer = outlineOf(rect, radius, segments);
    if (style.color.a > 0) {
        appendFill(out, outer, style.color);
    }

    if (style.borderThickness > 0.f && style.border.a > 0) {
        const float thickness = std::min(style.borderThickness, halfSmallerSide(rect));
        const Outline inner = outlineOf(rect.inset(thickness), radius - thickness, segments);
        appendRing(out, outer, style.border, inner, style.border);
    }
}

void appendLine(VertexList& out, sf::Vector2f from, sf::Vector2f to, const PartStyle& style) {
    if (!drawsLines(style) || from == to) {
        return;
    }
    const sf::Vector2f side = sideways(to - from, style.thickness * 0.5f);
    appendQuad(out, from + side, to + side, to - side, from - side, style.color);
}

void appendPolyline(VertexList& out, std::span<const sf::Vector2f> points, const PartStyle& style) {
    if (!drawsLines(style) || points.size() < 2) {
        return;
    }

    const float halfWidth = style.thickness * 0.5f;
    constexpr float miterLimit = 4.f; // how far a sharp bend may stick out, in half widths

    // The sideways offset at point `i`: at a bend it lies between the two segments' offsets and
    // is lengthened so both segments keep their thickness. That closes the gap a bend would leave.
    const auto offsetAt = [&](std::size_t i) -> sf::Vector2f {
        const sf::Vector2f before = i > 0 ? sideways(points[i] - points[i - 1], 1.f) : sf::Vector2f{};
        const sf::Vector2f after = i + 1 < points.size() ? sideways(points[i + 1] - points[i], 1.f) : sf::Vector2f{};

        const bool hasBefore = before != sf::Vector2f{};
        const bool hasAfter = after != sf::Vector2f{};
        if (!hasBefore && !hasAfter) {
            return {};
        }
        if (!hasBefore) {
            return after * halfWidth;
        }
        if (!hasAfter) {
            return before * halfWidth;
        }

        sf::Vector2f miter = before + after;
        const float length = std::hypot(miter.x, miter.y);
        if (length <= 1e-6f) { // the line turns back on itself
            return before * halfWidth;
        }
        miter = miter / length;
        const float stretch = 1.f / std::max(miter.x * before.x + miter.y * before.y, 1.f / miterLimit);
        return miter * (halfWidth * stretch);
    };

    sf::Vector2f previousOffset = offsetAt(0);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const sf::Vector2f offset = offsetAt(i);
        appendQuad(
            out,
            points[i - 1] + previousOffset,
            points[i] + offset,
            points[i] - offset,
            points[i - 1] - previousOffset,
            style.color
        );
        previousOffset = offset;
    }
}

} // namespace atpl::render
