#include "ui/render/shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace atpl::render {

namespace {

constexpr int maxCornerSegments = 32;
constexpr float maxCurveError = 0.25f;      // pixels between a straight piece and the true circle
constexpr float minFillHalfThickness = 1.f; // what an outline's gap always leaves of the fill, per side

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

/// The colour of every point of a filled box: one colour, or a gradient across the box.
/// A colour that changes evenly across a box also changes evenly across each triangle, so giving
/// every vertex the colour of its position draws the gradient exactly, at no extra cost.
class Fill {
public:
    explicit Fill(sf::Color color) :
        m_end(color) {}

    /// From `start` to `end` across `rect` in the gradient's direction.
    Fill(const FloatRect& rect, Gradient gradient, sf::Color start, sf::Color end) :
        m_gradient(gradient),
        m_start(start),
        m_end(end),
        m_rect(rect) {}

    /// The fill of a box in this style.
    Fill(const FloatRect& rect, const PartStyle& style) :
        Fill(rect, style.gradient, style.gradientStart, style.color) {}

    [[nodiscard]] sf::Color at(sf::Vector2f position) const {
        if (m_gradient == Gradient::None || m_rect.width() <= 0.f || m_rect.height() <= 0.f) {
            return m_end;
        }
        const float across = (position.x - m_rect.left()) / m_rect.width();
        const float down = (position.y - m_rect.top()) / m_rect.height();
        const float along = m_gradient == Gradient::Horizontal ? across
                            : m_gradient == Gradient::Vertical ? down
                                                               : (across + down) * 0.5f;
        const float t = std::clamp(along, 0.f, 1.f);
        const auto channel = [t](std::uint8_t from, std::uint8_t to) {
            return static_cast<std::uint8_t>(
                std::lround(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t)
            );
        };
        return { channel(m_start.r, m_end.r),
                 channel(m_start.g, m_end.g),
                 channel(m_start.b, m_end.b),
                 channel(m_start.a, m_end.a) };
    }

private:
    Gradient m_gradient = Gradient::None;
    sf::Color m_start;
    sf::Color m_end;
    FloatRect m_rect;
};

/// The area inside an outline.
void appendFill(VertexList& out, const Outline& outline, const Fill& fill) {
    const FloatRect& r = outline.rect;
    const auto vertex = [&](sf::Vector2f position) { out.push_back({ position, fill.at(position) }); };

    if (outline.segments == 0) { // sharp corners: two triangles
        vertex(r.topLeft());
        vertex(r.topRight());
        vertex(r.bottomRight());
        vertex(r.topLeft());
        vertex(r.bottomRight());
        vertex(r.bottomLeft());
        return;
    }

    // A fan around the centre.
    const sf::Vector2f center = r.center();
    const int count = outline.pointCount();
    sf::Vector2f previous = outline.point(count - 1);
    for (int i = 0; i < count; ++i) {
        const sf::Vector2f current = outline.point(i);
        vertex(center);
        vertex(previous);
        vertex(current);
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

/// The band between two outlines with the same number of points, each point coloured where it
/// lies in `fill`: an outline that fades along the box.
void appendRing(VertexList& out, const Outline& outer, const Outline& inner, const Fill& fill) {
    const int count = outer.pointCount();
    const auto vertex = [&](sf::Vector2f position) { out.push_back({ position, fill.at(position) }); };
    sf::Vector2f previousOuter = outer.point(count - 1);
    sf::Vector2f previousInner = inner.point(count - 1);

    for (int i = 0; i < count; ++i) {
        const sf::Vector2f currentOuter = outer.point(i);
        const sf::Vector2f currentInner = inner.point(i);
        vertex(previousOuter);
        vertex(currentOuter);
        vertex(currentInner);
        vertex(previousOuter);
        vertex(currentInner);
        vertex(previousInner);
        previousOuter = currentOuter;
        previousInner = currentInner;
    }
}

/// How many bands a shadow fades over: enough that its fade looks smooth.
constexpr int shadowBands = 6;

/// A soft shadow: solid well inside the box, fading to nothing `size` pixels outside it.
void appendShadow(VertexList& out, const FloatRect& rect, float radius, const Shadow& shadow) {
    const FloatRect base(rect.position() + shadow.offset, rect.size());
    const float size = shadow.size;
    const float inwards = std::min(size, halfSmallerSide(base));
    const int segments = cornerSegments(radius + size);

    // From `inwards` inside the box's edge to `size` outside it, the darkness falls off along
    // (1 - smoothstep)^2: soft at both ends, with a quarter of it left at the box's edge when the
    // shadow reaches as far in as out. (The reference project draws its card shadows like this,
    // with a shader.)
    const auto outlineAt = [&](float t) {
        const float offset = -inwards + (inwards + size) * t; // > 0: outside the box
        return outlineOf(base.inset(-offset), radius + offset, segments);
    };
    const auto colorAt = [&](float t) {
        const float smooth = t * t * (3.f - 2.f * t);
        const float left = 1.f - smooth;
        return withAlpha(shadow.color, left * left);
    };

    Outline inner = outlineAt(0.f);
    sf::Color innerColor = colorAt(0.f);
    appendFill(out, inner, Fill(innerColor));
    for (int band = 1; band <= shadowBands; ++band) {
        const float t = static_cast<float>(band) / static_cast<float>(shadowBands);
        const Outline outer = outlineAt(t);
        const sf::Color outerColor = colorAt(t);
        appendRing(out, outer, outerColor, inner, innerColor);
        inner = outer;
        innerColor = outerColor;
    }
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
    const bool fadingOutline = style.borderGradient != Gradient::None && style.borderStart.a > 0;
    const bool hasOutline = style.borderThickness > 0.f && (style.border.a > 0 || fadingOutline);
    const float thickness = hasOutline ? std::min(style.borderThickness, halfSmallerSide(rect)) : 0.f;

    // With a gap, the fill starts further in than the outline ends. On a very thin box the gap
    // gives way first, so that some fill always remains.
    const float room = std::max(halfSmallerSide(rect) - thickness - minFillHalfThickness, 0.f);
    const float gap = hasOutline ? std::clamp(style.borderGap, 0.f, room) : 0.f;
    const float fillInset = gap > 0.f ? thickness + gap : 0.f;

    const bool hasGradient = style.gradient != Gradient::None && style.gradientStart.a > 0;
    if (style.color.a > 0 || hasGradient) {
        const FloatRect fillRect = rect.inset(fillInset);
        appendFill(out, outlineOf(fillRect, radius - fillInset, segments), Fill(fillRect, style));
    }

    if (hasOutline) {
        const Outline inner = outlineOf(rect.inset(thickness), radius - thickness, segments);
        if (style.borderGradient != Gradient::None) {
            appendRing(out, outer, inner, Fill(rect, style.borderGradient, style.borderStart, style.border));
        } else {
            appendRing(out, outer, style.border, inner, style.border);
        }
    }
}

void appendLine(VertexList& out, sf::Vector2f from, sf::Vector2f to, const PartStyle& style) {
    if (!drawsLines(style) || from == to) {
        return;
    }
    const sf::Vector2f side = sideways(to - from, style.thickness * 0.5f);
    appendQuad(out, from + side, to + side, to - side, from - side, style.color);
}

void appendPolyline(
    VertexList& out, std::span<const sf::Vector2f> points, const PartStyle& style, sf::Vector2f offset
) {
    if (!drawsLines(style) || points.size() < 2) {
        return;
    }

    const float halfWidth = style.thickness * 0.5f;
    constexpr float miterLimit = 4.f; // how far a sharp bend may stick out, in half widths

    // The sideways offset at point `i`: at a bend it lies between the two segments' offsets and
    // is lengthened so both segments keep their thickness. That closes the gap a bend would leave.
    const auto sideAt = [&](std::size_t i) -> sf::Vector2f {
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

    sf::Vector2f previousSide = sideAt(0);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const sf::Vector2f side = sideAt(i);
        const sf::Vector2f from = points[i - 1] + offset;
        const sf::Vector2f to = points[i] + offset;
        appendQuad(out, from + previousSide, to + side, to - side, from - previousSide, style.color);
        previousSide = side;
    }
}

void appendArea(
    VertexList& out, std::span<const sf::Vector2f> points, float baseline, const PartStyle& style, sf::Vector2f offset
) {
    if (!style.shown || style.color.a == 0 || points.size() < 2) {
        return;
    }

    float furthest = 0.f;
    for (const sf::Vector2f point : points) {
        furthest = std::max(furthest, std::abs(point.y - baseline));
    }
    if (furthest <= 0.f) {
        return;
    }

    // The colour at a height: full strength at the furthest point of the curve, nothing at the
    // line. It depends on the height alone, so neighbouring pieces blend without visible seams.
    const auto vertexAt = [&](sf::Vector2f position) {
        sf::Color color = style.color;
        color.a = static_cast<std::uint8_t>(
            std::lround(static_cast<float>(style.color.a) * std::abs(position.y - baseline) / furthest)
        );
        out.push_back({ position + offset, color });
    };
    const auto onLine = [&](float x) { return sf::Vector2f{ x, baseline }; };

    for (std::size_t i = 1; i < points.size(); ++i) {
        const sf::Vector2f a = points[i - 1];
        const sf::Vector2f b = points[i];
        const float sideA = a.y - baseline;
        const float sideB = b.y - baseline;

        if (sideA * sideB < 0.f) {
            // The curve crosses the line between the two points: one triangle on each side.
            const float t = sideA / (sideA - sideB);
            const sf::Vector2f crossing = onLine(a.x + (b.x - a.x) * t);
            vertexAt(a);
            vertexAt(crossing);
            vertexAt(onLine(a.x));
            vertexAt(crossing);
            vertexAt(b);
            vertexAt(onLine(b.x));
        } else {
            vertexAt(a);
            vertexAt(b);
            vertexAt(onLine(b.x));
            vertexAt(a);
            vertexAt(onLine(b.x));
            vertexAt(onLine(a.x));
        }
    }
}

} // namespace atpl::render
