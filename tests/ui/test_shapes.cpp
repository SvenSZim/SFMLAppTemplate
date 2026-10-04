#include "ui/render/shapes.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <utility>
#include <vector>

using namespace atpl;
using atpl::render::appendArea;
using atpl::render::appendBox;
using atpl::render::appendLine;
using atpl::render::appendPolyline;
using atpl::render::cornerSegments;
using atpl::render::VertexList;
using Catch::Approx;

namespace {

constexpr float pi = std::numbers::pi_v<float>;
const sf::Color red(200, 30, 30);
const sf::Color blue(30, 30, 200);
const sf::Color black(0, 0, 0, 200);

PartStyle filled(sf::Color color, float radius = 0.f) {
    PartStyle style;
    style.color = color;
    style.radius = radius;
    return style;
}

PartStyle stroke(sf::Color color, float thickness) {
    PartStyle style;
    style.color = color;
    style.thickness = thickness;
    return style;
}

/// The area covered by the triangles of one colour (or of all colours).
float area(const VertexList& vertices, const sf::Color* color = nullptr) {
    float sum = 0.f;
    for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
        if (color != nullptr && vertices[i].color != *color) {
            continue;
        }
        const sf::Vector2f a = vertices[i].position;
        const sf::Vector2f b = vertices[i + 1].position;
        const sf::Vector2f c = vertices[i + 2].position;
        sum += std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
    }
    return sum;
}

FloatRect bounds(const VertexList& vertices) {
    float left = vertices.front().position.x;
    float top = vertices.front().position.y;
    float right = left;
    float bottom = top;
    for (const sf::Vertex& vertex : vertices) {
        left = std::min(left, vertex.position.x);
        top = std::min(top, vertex.position.y);
        right = std::max(right, vertex.position.x);
        bottom = std::max(bottom, vertex.position.y);
    }
    return { left, top, right - left, bottom - top };
}

bool hasVertexAt(const VertexList& vertices, sf::Vector2f position) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& vertex) {
        return std::abs(vertex.position.x - position.x) < 1e-3f && std::abs(vertex.position.y - position.y) < 1e-3f;
    });
}

} // namespace

// ----- Corners -----

TEST_CASE("larger corners get more pieces, within limits", "[ui][shapes]") {
    REQUIRE(cornerSegments(0.f) == 0);
    REQUIRE(cornerSegments(-3.f) == 0);
    REQUIRE(cornerSegments(0.1f) == 1);

    int previous = 0;
    for (const float radius : { 1.f, 2.f, 5.f, 10.f, 30.f, 100.f, 1000.f, 1.0e6f }) {
        const int segments = cornerSegments(radius);
        REQUIRE(segments >= 1);
        REQUIRE(segments >= previous);
        REQUIRE(segments <= 32);
        previous = segments;
    }
    REQUIRE(cornerSegments(1.0e6f) == 32);
}

TEST_CASE("corner pieces stay within a quarter pixel of the circle", "[ui][shapes]") {
    for (const float radius : { 2.f, 5.f, 10.f, 30.f, 100.f }) {
        const int segments = cornerSegments(radius);
        const float step = pi * 0.5f / static_cast<float>(segments);
        const float furthest = radius * (1.f - std::cos(step * 0.5f));
        REQUIRE(furthest <= 0.25f + 1e-4f);
    }
}

// ----- Boxes -----

TEST_CASE("a sharp box is two triangles covering the rectangle", "[ui][shapes]") {
    VertexList vertices;
    appendBox(vertices, FloatRect(10.f, 20.f, 100.f, 50.f), filled(red));

    REQUIRE(vertices.size() == 6);
    REQUIRE(area(vertices) == Approx(5000.f));
    REQUIRE(bounds(vertices) == FloatRect(10.f, 20.f, 100.f, 50.f));
    REQUIRE(std::all_of(vertices.begin(), vertices.end(), [](const sf::Vertex& v) { return v.color == red; }));
}

TEST_CASE("a rounded box stays inside its rectangle and loses only its corners", "[ui][shapes]") {
    const FloatRect rect(10.f, 20.f, 200.f, 100.f);
    const float radius = 20.f;
    VertexList vertices;
    appendBox(vertices, rect, filled(red, radius));

    REQUIRE(vertices.size() % 3 == 0);
    REQUIRE(vertices.size() == static_cast<std::size_t>(4 * (cornerSegments(radius) + 1) * 3));
    REQUIRE(bounds(vertices) == rect);

    // Rectangle minus the four corner squares plus the four quarter circles.
    const float expected = 200.f * 100.f - (4.f - pi) * radius * radius;
    REQUIRE(area(vertices) == Approx(expected).epsilon(0.002));

    // The corner itself is cut off; the points where the rounding starts are there.
    REQUIRE_FALSE(hasVertexAt(vertices, rect.topLeft()));
    REQUIRE(hasVertexAt(vertices, { rect.left(), rect.top() + radius }));
    REQUIRE(hasVertexAt(vertices, { rect.left() + radius, rect.top() }));
    REQUIRE(hasVertexAt(vertices, { rect.right(), rect.bottom() - radius }));
}

TEST_CASE("a radius larger than fits is limited to half the smaller side", "[ui][shapes]") {
    SECTION("a square becomes a circle") {
        VertexList vertices;
        appendBox(vertices, FloatRect(0.f, 0.f, 80.f, 80.f), filled(red, fullyRound));

        REQUIRE(bounds(vertices) == FloatRect(0.f, 0.f, 80.f, 80.f));
        // The straight pieces lie just inside the true circle, so the area is a little smaller.
        REQUIRE(area(vertices) < pi * 40.f * 40.f);
        REQUIRE(area(vertices) == Approx(pi * 40.f * 40.f).epsilon(0.01));
        for (const sf::Vertex& vertex : vertices) {
            const float distance = std::hypot(vertex.position.x - 40.f, vertex.position.y - 40.f);
            REQUIRE(distance <= 40.f + 1e-3f);
        }
    }

    SECTION("a wide box becomes a pill") {
        VertexList vertices;
        appendBox(vertices, FloatRect(0.f, 0.f, 200.f, 40.f), filled(red, fullyRound));

        REQUIRE(bounds(vertices) == FloatRect(0.f, 0.f, 200.f, 40.f));
        REQUIRE(area(vertices) == Approx(160.f * 40.f + pi * 20.f * 20.f).epsilon(0.005));
    }

    SECTION("a negative radius is a sharp box") {
        VertexList vertices;
        appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 50.f), filled(red, -5.f));
        REQUIRE(vertices.size() == 6);
    }
}

TEST_CASE("nothing is added for a box that cannot be seen", "[ui][shapes]") {
    VertexList vertices;

    PartStyle hidden = filled(red, 5.f);
    hidden.shown = false;
    appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 50.f), hidden);

    appendBox(vertices, FloatRect(0.f, 0.f, 0.f, 50.f), filled(red));
    appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 0.f), filled(red));
    appendBox(vertices, FloatRect(0.f, 0.f, -10.f, 50.f), filled(red));
    appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 50.f), filled(sf::Color::Transparent));

    REQUIRE(vertices.empty());
}

TEST_CASE("boxes are appended to what is already in the list", "[ui][shapes]") {
    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 10.f, 10.f), filled(red));
    appendBox(vertices, FloatRect(20.f, 0.f, 10.f, 10.f), filled(blue));

    REQUIRE(vertices.size() == 12);
    REQUIRE(vertices[0].color == red);
    REQUIRE(vertices[6].color == blue);
}

// ----- Gradients -----

TEST_CASE("a horizontal gradient runs from the start colour at the left to the colour at the right", "[ui][shapes]") {
    PartStyle style = filled(sf::Color(200, 100, 0, 255));
    style.gradient = Gradient::Horizontal;
    style.gradientStart = sf::Color(0, 0, 100, 55);

    VertexList vertices;
    appendBox(vertices, FloatRect(10.f, 0.f, 100.f, 20.f), style);

    REQUIRE(vertices.size() == 6);
    for (const sf::Vertex& vertex : vertices) {
        if (vertex.position.x == 10.f) {
            REQUIRE(vertex.color == sf::Color(0, 0, 100, 55));
        } else {
            REQUIRE(vertex.position.x == 110.f);
            REQUIRE(vertex.color == sf::Color(200, 100, 0, 255));
        }
    }
}

TEST_CASE("a vertical gradient runs from top to bottom", "[ui][shapes]") {
    PartStyle style = filled(sf::Color::White);
    style.gradient = Gradient::Vertical;
    style.gradientStart = sf::Color::Black;

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 50.f, 40.f, 200.f), style);

    for (const sf::Vertex& vertex : vertices) {
        REQUIRE(vertex.color == (vertex.position.y == 50.f ? sf::Color::Black : sf::Color::White));
    }
}

TEST_CASE("a diagonal gradient runs from the top-left corner to the bottom-right", "[ui][shapes]") {
    PartStyle style = filled(sf::Color(200, 200, 200, 255));
    style.gradient = Gradient::Diagonal;
    style.gradientStart = sf::Color(0, 0, 0, 255);

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 100.f, 20.f), style);

    for (const sf::Vertex& vertex : vertices) {
        const sf::Vector2f p = vertex.position;
        if (p == sf::Vector2f(0.f, 0.f)) {
            REQUIRE(vertex.color == sf::Color(0, 0, 0, 255));
        } else if (p == sf::Vector2f(100.f, 20.f)) {
            REQUIRE(vertex.color == sf::Color(200, 200, 200, 255));
        } else { // the other two corners: halfway
            REQUIRE(vertex.color == sf::Color(100, 100, 100, 255));
        }
    }
}

TEST_CASE("an outline can fade along the box", "[ui][shapes]") {
    PartStyle style = filled(sf::Color::Transparent);
    style.border = sf::Color(200, 200, 200);
    style.borderThickness = 2.f;
    style.borderGradient = Gradient::Horizontal;
    style.borderStart = sf::Color(0, 0, 0);

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 100.f, 20.f), style);

    REQUIRE_FALSE(vertices.empty());
    for (const sf::Vertex& vertex : vertices) {
        const auto expected = static_cast<std::uint8_t>(std::lround(vertex.position.x / 100.f * 200.f));
        REQUIRE(vertex.color == sf::Color(expected, expected, expected));
    }
}

TEST_CASE("an outline that fades in from nothing is still drawn", "[ui][shapes]") {
    PartStyle style = filled(sf::Color::Transparent);
    style.border = sf::Color::Transparent; // it ends invisible ...
    style.borderThickness = 1.f;
    style.borderGradient = Gradient::Vertical;
    style.borderStart = red; // ... but starts in red

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 50.f), style);
    REQUIRE_FALSE(vertices.empty());
    REQUIRE(std::any_of(vertices.begin(), vertices.end(), [](const sf::Vertex& v) { return v.color == red; }));
}

TEST_CASE("in a rounded box every point has the gradient's colour for its position", "[ui][shapes]") {
    PartStyle style = filled(sf::Color(200, 200, 200), 12.f);
    style.gradient = Gradient::Horizontal;
    style.gradientStart = sf::Color(0, 0, 0);

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 100.f, 40.f), style);

    for (const sf::Vertex& vertex : vertices) {
        const float expected = vertex.position.x / 100.f * 200.f;
        REQUIRE(static_cast<float>(vertex.color.r) == Approx(expected).margin(0.51));
    }
    REQUIRE(hasVertexAt(vertices, { 50.f, 20.f })); // the centre of the fan
}

TEST_CASE("a gradient can fade in from nothing", "[ui][shapes]") {
    // The end colour is transparent, the start is not: the box must still be drawn.
    PartStyle style = filled(sf::Color(255, 255, 255, 0));
    style.gradient = Gradient::Horizontal;
    style.gradientStart = sf::Color(255, 255, 255, 255);

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 50.f, 10.f), style);

    REQUIRE(vertices.size() == 6);
}

TEST_CASE("lines ignore a gradient and use the colour", "[ui][shapes]") {
    PartStyle style = stroke(red, 2.f);
    style.gradient = Gradient::Horizontal;
    style.gradientStart = blue;

    VertexList vertices;
    appendLine(vertices, { 0.f, 0.f }, { 50.f, 0.f }, style);

    REQUIRE(std::all_of(vertices.begin(), vertices.end(), [](const sf::Vertex& v) { return v.color == red; }));
}

// ----- Outlines -----

TEST_CASE("an outline is drawn inside the box, over the fill", "[ui][shapes]") {
    const FloatRect rect(0.f, 0.f, 100.f, 60.f);
    PartStyle style = filled(red);
    style.border = blue;
    style.borderThickness = 4.f;

    VertexList vertices;
    appendBox(vertices, rect, style);

    REQUIRE(bounds(vertices) == rect); // not a pixel larger
    REQUIRE(area(vertices, &red) == Approx(100.f * 60.f));
    REQUIRE(area(vertices, &blue) == Approx(100.f * 60.f - 92.f * 52.f));

    // Fill first, outline after it.
    REQUIRE(vertices.front().color == red);
    REQUIRE(vertices.back().color == blue);
}

TEST_CASE("an outline follows rounded corners", "[ui][shapes]") {
    const FloatRect rect(0.f, 0.f, 100.f, 60.f);
    PartStyle style = filled(sf::Color::Transparent, 15.f); // outline only
    style.border = blue;
    style.borderThickness = 3.f;

    VertexList vertices;
    appendBox(vertices, rect, style);

    const float outer = 100.f * 60.f - (4.f - pi) * 15.f * 15.f;
    const float inner = 94.f * 54.f - (4.f - pi) * 12.f * 12.f;
    REQUIRE(area(vertices) == Approx(outer - inner).epsilon(0.01));
    REQUIRE(bounds(vertices) == rect);
}

TEST_CASE("a gap moves the fill inwards and leaves the outline at the edge", "[ui][shapes]") {
    const FloatRect rect(0.f, 0.f, 100.f, 60.f);
    PartStyle style = filled(red);
    style.border = blue;
    style.borderThickness = 2.f;
    style.borderGap = 3.f;

    VertexList vertices;
    appendBox(vertices, rect, style);

    REQUIRE(bounds(vertices) == rect);                                    // the box is no larger
    REQUIRE(area(vertices, &blue) == Approx(100.f * 60.f - 96.f * 56.f)); // outline: 2 wide, at the edge
    REQUIRE(area(vertices, &red) == Approx(90.f * 50.f));                 // fill: 2 + 3 further in on every side
    REQUIRE(style.contentInset() == 5.f);
}

TEST_CASE("a gap follows rounded corners", "[ui][shapes]") {
    PartStyle style = filled(red, 20.f);
    style.border = blue;
    style.borderThickness = 2.f;
    style.borderGap = 3.f;

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 100.f, 60.f), style);

    // The fill is a rounded box of its own, with the radius reduced by the inset.
    const float fill = 90.f * 50.f - (4.f - pi) * 15.f * 15.f;
    REQUIRE(area(vertices, &red) == Approx(fill).epsilon(0.005));
}

TEST_CASE("on a thin box the gap gives way before the fill disappears", "[ui][shapes]") {
    PartStyle style = filled(red);
    style.border = blue;
    style.borderThickness = 1.f;
    style.borderGap = 4.f;

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 100.f, 6.f), style); // room for a gap of 1 at most

    REQUIRE(area(vertices, &red) > 0.f);
    REQUIRE(area(vertices, &red) == Approx(96.f * 2.f)); // 1 outline + 1 gap on every side
}

TEST_CASE("a gap means nothing without an outline", "[ui][shapes]") {
    PartStyle style = filled(red);
    style.borderGap = 5.f;

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 40.f, 20.f), style);

    REQUIRE(area(vertices, &red) == Approx(40.f * 20.f));
    REQUIRE(style.contentInset() == 0.f);
}

TEST_CASE("an outline thicker than the box fills it", "[ui][shapes]") {
    PartStyle style = filled(sf::Color::Transparent);
    style.border = blue;
    style.borderThickness = 500.f;

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 40.f, 20.f), style);

    REQUIRE(area(vertices) == Approx(40.f * 20.f));
    REQUIRE(bounds(vertices) == FloatRect(0.f, 0.f, 40.f, 20.f));
}

// ----- Shadows -----

TEST_CASE("a shadow lies under the box, is shifted, and fades out", "[ui][shapes]") {
    const FloatRect rect(100.f, 100.f, 200.f, 80.f);
    PartStyle style = filled(red, 10.f);
    style.shadow = { .offset = { 0.f, 6.f }, .size = 12.f, .color = black };

    VertexList vertices;
    appendBox(vertices, rect, style);

    // It reaches `size` beyond the shifted box on every side.
    REQUIRE(bounds(vertices) == FloatRect(88.f, 94.f, 224.f, 104.f));

    // Shadow first, so the box is drawn over it.
    REQUIRE(vertices.front().color == black);
    REQUIRE(vertices.back().color == red);

    // Fully clear at its outer edge, and nowhere darker than asked for.
    std::uint8_t darkest = 0;
    bool clearAtRim = false;
    for (const sf::Vertex& vertex : vertices) {
        if (vertex.color == red) {
            continue;
        }
        darkest = std::max(darkest, vertex.color.a);
        if (vertex.position.x == 88.f) {
            clearAtRim = clearAtRim || vertex.color.a == 0;
            REQUIRE(vertex.color.a == 0);
        }
    }
    REQUIRE(clearAtRim);
    REQUIRE(darkest == black.a);
}

TEST_CASE("a shadow fades smoothly, darkest inside and lighter with every step outwards", "[ui][shapes]") {
    const FloatRect rect(0.f, 0.f, 200.f, 200.f);
    PartStyle style = filled(sf::Color::Transparent);
    style.shadow = { .offset = {}, .size = 20.f, .color = sf::Color(0, 0, 0, 200) };

    VertexList vertices;
    appendBox(vertices, rect, style);

    // Every vertex by how far outside the box's edge it lies (negative: inside); further out is
    // never darker.
    const auto outside = [&](sf::Vector2f p) {
        const sf::Vector2f q(std::abs(p.x - 100.f) - 100.f, std::abs(p.y - 100.f) - 100.f);
        const sf::Vector2f outer(std::max(q.x, 0.f), std::max(q.y, 0.f));
        return std::hypot(outer.x, outer.y) + std::min(std::max(q.x, q.y), 0.f);
    };
    std::vector<std::pair<float, std::uint8_t>> byDistance;
    for (const sf::Vertex& vertex : vertices) {
        const float distance = std::round(outside(vertex.position) * 100.f) / 100.f;
        if (distance < -20.f) {
            REQUIRE(vertex.color.a == 200); // the core: full, all the way in
            continue;
        }
        byDistance.emplace_back(distance, vertex.color.a);
    }
    std::sort(byDistance.begin(), byDistance.end());
    REQUIRE(byDistance.front().first == Approx(-20.f));
    REQUIRE(byDistance.front().second == 200); // full where it starts, 20 px inside
    REQUIRE(byDistance.back().first == Approx(20.f));
    REQUIRE(byDistance.back().second == 0); // clear at the rim, 20 px outside
    std::set<float> distances;
    for (std::size_t i = 1; i < byDistance.size(); ++i) {
        distances.insert(byDistance[i].first);
        if (byDistance[i].first > byDistance[i - 1].first + 0.05f) {
            REQUIRE(byDistance[i].second <= byDistance[i - 1].second);
        }
    }
    REQUIRE(distances.size() >= 7); // the core's edge and six bands

    // At the box's edge a quarter is left: (1 - smoothstep(0.5))^2.
    for (const auto& [distance, alpha] : byDistance) {
        if (distance == 0.f) {
            REQUIRE(alpha == 50);
        }
    }
}

TEST_CASE("a shadow without size or without colour adds nothing", "[ui][shapes]") {
    const FloatRect rect(0.f, 0.f, 50.f, 50.f);
    VertexList plain;
    appendBox(plain, rect, filled(red));

    PartStyle noSize = filled(red);
    noSize.shadow = { .offset = { 0.f, 4.f }, .size = 0.f, .color = black };
    VertexList first;
    appendBox(first, rect, noSize);

    PartStyle noColor = filled(red);
    noColor.shadow = { .offset = { 0.f, 4.f }, .size = 10.f, .color = sf::Color::Transparent };
    VertexList second;
    appendBox(second, rect, noColor);

    REQUIRE(first.size() == plain.size());
    REQUIRE(second.size() == plain.size());
}

TEST_CASE("a shadow larger than the box still gives sound triangles", "[ui][shapes]") {
    PartStyle style = filled(red, 4.f);
    style.shadow = { .offset = {}, .size = 100.f, .color = black };

    VertexList vertices;
    appendBox(vertices, FloatRect(0.f, 0.f, 20.f, 10.f), style);

    REQUIRE(vertices.size() % 3 == 0);
    REQUIRE(bounds(vertices) == FloatRect(-100.f, -100.f, 220.f, 210.f));
    for (const sf::Vertex& vertex : vertices) {
        REQUIRE(std::isfinite(vertex.position.x));
        REQUIRE(std::isfinite(vertex.position.y));
    }
}

// ----- Lines -----

TEST_CASE("a line is a rectangle of its thickness around the two points", "[ui][shapes]") {
    VertexList vertices;
    appendLine(vertices, { 10.f, 50.f }, { 110.f, 50.f }, stroke(red, 4.f));

    REQUIRE(vertices.size() == 6);
    REQUIRE(bounds(vertices) == FloatRect(10.f, 48.f, 100.f, 4.f));
    REQUIRE(area(vertices) == Approx(400.f));
}

TEST_CASE("a slanted line keeps its thickness", "[ui][shapes]") {
    VertexList vertices;
    appendLine(vertices, { 0.f, 0.f }, { 30.f, 40.f }, stroke(red, 2.f));

    REQUIRE(area(vertices) == Approx(50.f * 2.f)); // length 50
}

TEST_CASE("nothing is added for a line that cannot be seen", "[ui][shapes]") {
    VertexList vertices;
    appendLine(vertices, { 5.f, 5.f }, { 5.f, 5.f }, stroke(red, 2.f));
    appendLine(vertices, { 0.f, 0.f }, { 9.f, 0.f }, stroke(red, 0.f));
    appendLine(vertices, { 0.f, 0.f }, { 9.f, 0.f }, stroke(sf::Color::Transparent, 2.f));

    PartStyle hidden = stroke(red, 2.f);
    hidden.shown = false;
    appendLine(vertices, { 0.f, 0.f }, { 9.f, 0.f }, hidden);

    REQUIRE(vertices.empty());
}

// ----- Polylines -----

TEST_CASE("a polyline has one rectangle per piece", "[ui][shapes]") {
    const std::array<sf::Vector2f, 4> points = { { { 0.f, 0.f }, { 10.f, 0.f }, { 20.f, 5.f }, { 30.f, 5.f } } };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 2.f));

    REQUIRE(vertices.size() == 3 * 6);
}

TEST_CASE("a straight polyline equals one line", "[ui][shapes]") {
    const std::array<sf::Vector2f, 3> points = { { { 0.f, 10.f }, { 50.f, 10.f }, { 100.f, 10.f } } };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 4.f));

    REQUIRE(bounds(vertices) == FloatRect(0.f, 8.f, 100.f, 4.f));
    REQUIRE(area(vertices) == Approx(400.f));
}

TEST_CASE("the pieces of a polyline meet without a gap at a bend", "[ui][shapes]") {
    // A right angle: along x, then down.
    const std::array<sf::Vector2f, 3> points = { { { 0.f, 0.f }, { 50.f, 0.f }, { 50.f, 50.f } } };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 4.f));

    // Both pieces use the same two points at the bend: the outer and the inner corner.
    REQUIRE(hasVertexAt(vertices, { 52.f, -2.f }));
    REQUIRE(hasVertexAt(vertices, { 48.f, 2.f }));

    const auto timesUsed = [&](sf::Vector2f position) {
        return std::count_if(vertices.begin(), vertices.end(), [&](const sf::Vertex& vertex) {
            return std::abs(vertex.position.x - position.x) < 1e-3f && std::abs(vertex.position.y - position.y) < 1e-3f;
        });
    };
    REQUIRE(timesUsed({ 52.f, -2.f }) >= 2);
    REQUIRE(timesUsed({ 48.f, 2.f }) >= 2);
}

TEST_CASE("a very sharp bend does not stick out without limit", "[ui][shapes]") {
    // Almost a full turn back.
    const std::array<sf::Vector2f, 3> points = { { { 0.f, 0.f }, { 100.f, 0.f }, { 0.f, 1.f } } };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 2.f));

    // Half the thickness times the limit of 4, plus a little.
    REQUIRE(bounds(vertices).right() <= 100.f + 4.f + 0.01f);
}

TEST_CASE("a polyline copes with repeated points and with turning back", "[ui][shapes]") {
    const std::array<sf::Vector2f, 5> points = {
        { { 0.f, 0.f }, { 10.f, 0.f }, { 10.f, 0.f }, { 20.f, 0.f }, { 0.f, 0.f } }
    };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 2.f));

    REQUIRE(vertices.size() == 4 * 6);
    for (const sf::Vertex& vertex : vertices) {
        REQUIRE(std::isfinite(vertex.position.x));
        REQUIRE(std::isfinite(vertex.position.y));
    }
}

TEST_CASE("nothing is added for a polyline of fewer than two points", "[ui][shapes]") {
    VertexList vertices;
    const std::array<sf::Vector2f, 1> one = { { { 1.f, 1.f } } };

    appendPolyline(vertices, {}, stroke(red, 2.f));
    appendPolyline(vertices, one, stroke(red, 2.f));

    REQUIRE(vertices.empty());
}

TEST_CASE("a polyline can be moved as a whole", "[ui][shapes]") {
    const std::array<sf::Vector2f, 2> points = { { { 0.f, 0.f }, { 10.f, 0.f } } };
    VertexList vertices;
    appendPolyline(vertices, points, stroke(red, 2.f), { 100.f, 50.f });

    REQUIRE(bounds(vertices) == FloatRect(100.f, 49.f, 10.f, 2.f));
}

// ----- Areas -----

TEST_CASE("an area fills between a curve and a line, fading to nothing at the line", "[ui][shapes]") {
    // A curve above the line (smaller y is higher on screen).
    const std::array<sf::Vector2f, 3> points = { { { 0.f, 60.f }, { 50.f, 20.f }, { 100.f, 60.f } } };
    VertexList vertices;
    appendArea(vertices, points, 100.f, filled(sf::Color(200, 30, 30, 200)));

    REQUIRE(vertices.size() == 2 * 6);
    REQUIRE(bounds(vertices) == FloatRect(0.f, 20.f, 100.f, 80.f));
    // Two trapezoids: between heights 40 and 80, 50 wide each.
    REQUIRE(area(vertices) == Approx(2.f * 50.f * (40.f + 80.f) / 2.f));

    for (const sf::Vertex& vertex : vertices) {
        if (vertex.position.y == 100.f) {
            REQUIRE(vertex.color.a == 0); // on the line
        } else if (vertex.position.y == 20.f) {
            REQUIRE(vertex.color.a == 200); // the point furthest from the line: full strength
        } else {
            REQUIRE(vertex.color.a == 100); // half as far: half as strong
        }
        REQUIRE(vertex.color.r == 200);
    }
}

TEST_CASE("the strength of an area depends only on the distance from the line", "[ui][shapes]") {
    // Uneven steps along x: points at the same height must still get the same strength.
    const std::array<sf::Vector2f, 4> points = { { { 0.f, 10.f }, { 5.f, 40.f }, { 80.f, 40.f }, { 81.f, 10.f } } };
    VertexList vertices;
    appendArea(vertices, points, 50.f, filled(sf::Color(0, 0, 0, 240)));

    for (const sf::Vertex& vertex : vertices) {
        const float expected = 240.f * (50.f - vertex.position.y) / 40.f;
        REQUIRE(static_cast<float>(vertex.color.a) == Approx(expected).margin(0.51));
    }
}

TEST_CASE("an area fills both sides where the curve crosses the line", "[ui][shapes]") {
    // From 20 above the line to 20 below it: crosses halfway.
    const std::array<sf::Vector2f, 2> points = { { { 0.f, 30.f }, { 40.f, 70.f } } };
    VertexList vertices;
    appendArea(vertices, points, 50.f, filled(red));

    REQUIRE(vertices.size() == 6);
    REQUIRE(hasVertexAt(vertices, { 20.f, 50.f }));               // the crossing
    REQUIRE(area(vertices) == Approx(2.f * (20.f * 20.f / 2.f))); // a triangle on each side
    REQUIRE(bounds(vertices) == FloatRect(0.f, 30.f, 40.f, 40.f));
}

TEST_CASE("an area can be moved as a whole", "[ui][shapes]") {
    const std::array<sf::Vector2f, 2> points = { { { 0.f, 0.f }, { 10.f, 0.f } } };
    VertexList vertices;
    appendArea(vertices, points, 20.f, filled(red), { 100.f, 50.f });

    REQUIRE(bounds(vertices) == FloatRect(100.f, 50.f, 10.f, 20.f));
}

TEST_CASE("nothing is added for an area that cannot be seen", "[ui][shapes]") {
    const std::array<sf::Vector2f, 2> flat = { { { 0.f, 50.f }, { 10.f, 50.f } } };
    const std::array<sf::Vector2f, 2> curve = { { { 0.f, 10.f }, { 10.f, 20.f } } };
    const std::array<sf::Vector2f, 1> one = { { { 0.f, 10.f } } };
    VertexList vertices;

    appendArea(vertices, flat, 50.f, filled(red)); // the curve lies on the line
    appendArea(vertices, one, 50.f, filled(red));
    appendArea(vertices, curve, 50.f, filled(sf::Color::Transparent));
    PartStyle hidden = filled(red);
    hidden.shown = false;
    appendArea(vertices, curve, 50.f, hidden);

    REQUIRE(vertices.empty());
}
