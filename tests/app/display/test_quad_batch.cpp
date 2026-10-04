#include "atpl/app/quad_batch.hpp"
#include "atpl/app/resources.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace atpl;

namespace {

sf::Image drawn(const QuadBatch& batch) {
    sf::RenderTexture target({ 40u, 20u });
    target.clear(sf::Color::Black);
    target.draw(batch);
    target.display();
    return target.getTexture().copyToImage();
}

} // namespace

TEST_CASE("a batch draws its quads in their colours", "[app][quad_batch][display]") {
    QuadBatch batch;
    batch.add(FloatRect(0.f, 0.f, 20.f, 20.f), sf::Color::Red);
    batch.add(FloatRect(20.f, 0.f, 20.f, 20.f), sf::Color::Blue);
    const sf::Image image = drawn(batch);
    REQUIRE(image.getPixel({ 5u, 10u }) == sf::Color::Red);
    REQUIRE(image.getPixel({ 35u, 10u }) == sf::Color::Blue);

    QuadBatch empty;
    REQUIRE(drawn(empty).getPixel({ 5u, 10u }) == sf::Color::Black);
}

TEST_CASE("a textured batch shows its texture, tinted", "[app][quad_batch][display]") {
    const Resources resources = Resources::nextToExecutable();
    const sf::Texture& dot = resources.texture("textures/particle.png");
    REQUIRE(dot.getSize() == sf::Vector2u(64u, 64u));
    REQUIRE(dot.isSmooth()); // the default options

    QuadBatch batch(&dot);
    batch.add(FloatRect(0.f, 0.f, 20.f, 20.f), sf::Color::Green); // the whole texture
    const sf::Image image = drawn(batch);
    REQUIRE(image.getPixel({ 10u, 10u }) == sf::Color::Green); // the disc's middle, tinted
    REQUIRE(image.getPixel({ 0u, 0u }) == sf::Color::Black);   // outside the disc: transparent
    REQUIRE(image.getPixel({ 30u, 10u }) == sf::Color::Black); // no quad there
}

TEST_CASE(
    "a texture asked for by name is loaded once, with the options of the first call", "[app][resources][display]"
) {
    const Resources resources = Resources::nextToExecutable();
    const sf::Texture& sharp = resources.texture("textures/particle.png", { .smooth = false, .repeated = true });
    REQUIRE_FALSE(sharp.isSmooth());
    REQUIRE(sharp.isRepeated());
    const sf::Texture& again = resources.texture("textures/particle.png"); // other options: the same texture
    REQUIRE(&again == &sharp);
    REQUIRE_FALSE(again.isSmooth());

    const sf::Texture fresh = resources.loadTexture("textures/particle.png");
    REQUIRE(fresh.isSmooth());
    REQUIRE_THROWS_AS(resources.texture("textures/missing.png"), ResourceError);
    REQUIRE_THROWS_AS(resources.loadTexture("fonts/OFL.txt"), ResourceError);
}
