#include "ui/render/shapes.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using namespace atpl;
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
