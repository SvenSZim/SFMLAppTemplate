#include "atpl/app/quad_batch.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

using namespace atpl;
using Catch::Approx;

namespace {

/// The corner `corner` (0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left) of quad `quad`,
/// as the vertices hold it: two triangles, corners 0 1 2 and 0 2 3.
const sf::Vertex& corner(const QuadBatch& batch, std::size_t quad, std::size_t corner) {
    constexpr std::array<std::size_t, 4> firstVertexOf{ 0, 1, 2, 5 };
    return batch.vertices()[quad * QuadBatch::verticesPerQuad + firstVertexOf[corner]];
}

} // namespace

TEST_CASE("a quad is two triangles of six vertices", "[app][quad_batch]") {
    QuadBatch batch;
    REQUIRE(batch.empty());
    REQUIRE(batch.add(FloatRect(10.f, 20.f, 30.f, 40.f), sf::Color::Red) == 0);
    REQUIRE(batch.size() == 1);
    REQUIRE(batch.vertices().size() == 6);

    const auto at = [&](std::size_t i) { return batch.vertices()[i].position; };
    REQUIRE(at(0) == sf::Vector2f(10.f, 20.f)); // top-left
    REQUIRE(at(1) == sf::Vector2f(40.f, 20.f)); // top-right
    REQUIRE(at(2) == sf::Vector2f(40.f, 60.f)); // bottom-right
    REQUIRE(at(3) == sf::Vector2f(10.f, 20.f));
    REQUIRE(at(4) == sf::Vector2f(40.f, 60.f));
    REQUIRE(at(5) == sf::Vector2f(10.f, 60.f)); // bottom-left
    for (const sf::Vertex& vertex : batch.vertices()) {
        REQUIRE(vertex.color == sf::Color::Red);
        REQUIRE(vertex.texCoords == sf::Vector2f(0.f, 0.f)); // no texture
    }
}

TEST_CASE("added quads get the next indices", "[app][quad_batch]") {
    QuadBatch batch;
    batch.reserve(3);
    REQUIRE(batch.add(FloatRect(0.f, 0.f, 1.f, 1.f)) == 0);
    REQUIRE(batch.add(FloatRect(1.f, 0.f, 1.f, 1.f)) == 1);
    REQUIRE(batch.add({ 5.f, 5.f }, { 2.f, 2.f }, sf::degrees(0.f)) == 2);
    REQUIRE(batch.size() == 3);
    REQUIRE(corner(batch, 2, 0).position == sf::Vector2f(4.f, 4.f));
    REQUIRE(corner(batch, 2, 2).position == sf::Vector2f(6.f, 6.f));

    batch.clear();
    REQUIRE(batch.empty());
    REQUIRE(batch.add(FloatRect(0.f, 0.f, 1.f, 1.f)) == 0);
}

TEST_CASE("a turned quad turns around its centre", "[app][quad_batch]") {
    QuadBatch batch;
    batch.add({ 10.f, 10.f }, { 4.f, 2.f }, sf::degrees(90.f));
    // A quarter turn clockwise on screen (y down): the top-left corner (-2, -1) goes to (1, -2).
    REQUIRE(corner(batch, 0, 0).position.x == Approx(11.f));
    REQUIRE(corner(batch, 0, 0).position.y == Approx(8.f));
    REQUIRE(corner(batch, 0, 2).position.x == Approx(9.f));
    REQUIRE(corner(batch, 0, 2).position.y == Approx(12.f));
}

TEST_CASE("texture rectangles become the corners' texture points", "[app][quad_batch]") {
    QuadBatch batch;
    batch.add(FloatRect(0.f, 0.f, 10.f, 10.f), FloatRect(16.f, 0.f, 16.f, 32.f), sf::Color::White);
    REQUIRE(corner(batch, 0, 0).texCoords == sf::Vector2f(16.f, 0.f));
    REQUIRE(corner(batch, 0, 1).texCoords == sf::Vector2f(32.f, 0.f));
    REQUIRE(corner(batch, 0, 2).texCoords == sf::Vector2f(32.f, 32.f));
    REQUIRE(corner(batch, 0, 3).texCoords == sf::Vector2f(16.f, 32.f));
}

TEST_CASE("quads can be changed in place", "[app][quad_batch]") {
    QuadBatch batch;
    batch.resize(3);
    REQUIRE(batch.size() == 3);
    for (const sf::Vertex& vertex : batch.vertices()) {
        REQUIRE(vertex.color == sf::Color::Transparent); // nothing shows until set
    }

    batch.set(1, FloatRect(2.f, 2.f, 2.f, 2.f), sf::Color::Green);
    REQUIRE(corner(batch, 1, 0).position == sf::Vector2f(2.f, 2.f));
    REQUIRE(corner(batch, 0, 0).color == sf::Color::Transparent); // the others untouched

    batch.setColor(1, sf::Color::Blue);
    REQUIRE(corner(batch, 1, 3).color == sf::Color::Blue);
    REQUIRE(corner(batch, 1, 0).position == sf::Vector2f(2.f, 2.f)); // only the colour

    batch.setColors(1, { sf::Color::Red, sf::Color::Green, sf::Color::Blue, sf::Color::Yellow });
    REQUIRE(corner(batch, 1, 0).color == sf::Color::Red);
    REQUIRE(corner(batch, 1, 1).color == sf::Color::Green);
    REQUIRE(corner(batch, 1, 2).color == sf::Color::Blue);
    REQUIRE(corner(batch, 1, 3).color == sf::Color::Yellow);
    for (std::size_t i = 0; i < QuadBatch::verticesPerQuad; ++i) { // both triangles agree
        const sf::Vertex& vertex = batch.vertices()[QuadBatch::verticesPerQuad + i];
        constexpr std::array<std::size_t, 6> cornerOf{ 0, 1, 2, 0, 2, 3 };
        REQUIRE(vertex.color == corner(batch, 1, cornerOf[i]).color);
    }

    batch.set(
        2,
        { { { 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 2.f } } },
        { sf::Color::White, sf::Color::White, sf::Color::White, sf::Color::Black }
    );
    REQUIRE(corner(batch, 2, 3).position == sf::Vector2f(0.f, 2.f));
    REQUIRE(corner(batch, 2, 3).color == sf::Color::Black);
}
