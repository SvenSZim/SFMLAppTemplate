// These tests draw into an off-screen target and look at the pixels, so they need a display
// (a virtual one will do). They carry the CTest label "display".

#include "ui/render/panel_batch.hpp"
#include "ui/render/renderer.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using namespace atpl;
using atpl::render::FrameStats;
using atpl::render::PanelBatch;
using atpl::render::Renderer;

namespace {

const sf::Color clear(10, 10, 10);
const sf::Color red(200, 30, 30);
const sf::Color green(30, 200, 30);
const sf::Color blue(30, 30, 200);
const sf::Color white(255, 255, 255);

PartStyle solid(sf::Color color) {
    PartStyle style;
    style.color = color;
    return style;
}

/// A panel that is one box of one colour, 60 by 40.
void paintPlain(PanelBatch& batch, sf::Color color) {
    const auto layers = batch.rebuild();
    render::appendBox(layers.frame.shapes(), FloatRect(0.f, 0.f, 60.f, 40.f), solid(color));
}

/// Paints the batch only if it is dirty, as the UI will.
void paintIfDirty(PanelBatch& batch, sf::Color color) {
    if (batch.isDirty()) {
        paintPlain(batch, color);
    }
}

struct Scene {
    sf::RenderTexture target{ { 200u, 150u } }; // no anti-aliasing: pixels are exact
    Renderer renderer;

    FrameStats draw(std::span<const PanelBatch* const> panels, const PanelBatch* overlay = nullptr) {
        target.clear(clear);
        const FrameStats stats = renderer.draw(target, panels, overlay);
        target.display();
        image = target.getTexture().copyToImage();
        return stats;
    }

    [[nodiscard]] sf::Color at(unsigned x, unsigned y) const { return image.getPixel({ x, y }); }

    sf::Image image;
};

} // namespace

TEST_CASE("a scene of several panels is drawn, each at its place", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch first;
    PanelBatch second;
    PanelBatch third;
    paintPlain(first, red);
    paintPlain(second, green);
    paintPlain(third, blue);
    first.setPosition({ 10.f, 10.f });
    second.setPosition({ 120.f, 10.f });
    third.setPosition({ 10.f, 90.f });

    const std::array<const PanelBatch*, 3> panels = { &first, &second, &third };
    const FrameStats stats = scene.draw(panels);

    REQUIRE(scene.at(40, 30) == red);
    REQUIRE(scene.at(150, 30) == green);
    REQUIRE(scene.at(40, 110) == blue);
    REQUIRE(scene.at(100, 30) == clear);  // between the panels
    REQUIRE(scene.at(150, 110) == clear); // where no panel is

    REQUIRE(stats.panelsDrawn == 3);
    REQUIRE(stats.drawCalls == 3); // one per panel
    REQUIRE(stats.triangles == 6);
}

TEST_CASE("what is listed later is drawn on top, and the overlay on top of everything", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch below;
    PanelBatch above;
    PanelBatch overlay;
    paintPlain(below, red);
    paintPlain(above, green);
    paintPlain(overlay, white);
    below.setPosition({ 10.f, 10.f });
    above.setPosition({ 40.f, 30.f });   // overlaps `below`
    overlay.setPosition({ 70.f, 50.f }); // overlaps `above`

    const std::array<const PanelBatch*, 2> panels = { &below, &above };
    scene.draw(panels, &overlay);

    REQUIRE(scene.at(20, 20) == red);   // only `below`
    REQUIRE(scene.at(55, 40) == green); // both: `above` wins
    REQUIRE(scene.at(85, 60) == white); // `above` and the overlay: the overlay wins

    // The same batches in the other order: now `below` covers `above`.
    const std::array<const PanelBatch*, 2> reversed = { &above, &below };
    scene.draw(reversed);
    REQUIRE(scene.at(55, 40) == red);
}

TEST_CASE("only the panel that changed is rebuilt", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch first;
    PanelBatch second;
    PanelBatch third;
    first.setPosition({ 10.f, 10.f });
    second.setPosition({ 120.f, 10.f });
    third.setPosition({ 10.f, 90.f });
    const std::array<const PanelBatch*, 3> panels = { &first, &second, &third };

    const auto frame = [&](sf::Color secondColor) {
        paintIfDirty(first, red);
        paintIfDirty(second, secondColor);
        paintIfDirty(third, blue);
        scene.draw(panels);
    };

    frame(green); // the first frame builds everything
    REQUIRE(first.rebuildCount() == 1);
    REQUIRE(second.rebuildCount() == 1);
    REQUIRE(third.rebuildCount() == 1);

    frame(green); // nothing changed: nothing is rebuilt, and the picture is the same
    frame(green);
    REQUIRE(first.rebuildCount() == 1);
    REQUIRE(second.rebuildCount() == 1);
    REQUIRE(third.rebuildCount() == 1);
    REQUIRE(scene.at(150, 30) == green);

    second.markDirty(); // something in the second panel changed
    frame(white);
    REQUIRE(first.rebuildCount() == 1);
    REQUIRE(second.rebuildCount() == 2);
    REQUIRE(third.rebuildCount() == 1);
    REQUIRE(scene.at(150, 30) == white);
    REQUIRE(scene.at(40, 30) == red);
}

TEST_CASE("a moved panel is drawn at its new place without being rebuilt", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch batch;
    paintPlain(batch, red);
    batch.setPosition({ 10.f, 10.f });
    const std::array<const PanelBatch*, 1> panels = { &batch };

    scene.draw(panels);
    REQUIRE(scene.at(40, 30) == red);

    batch.setPosition({ 120.f, 90.f });
    scene.draw(panels);

    REQUIRE(scene.at(40, 30) == clear);
    REQUIRE(scene.at(150, 110) == red);
    REQUIRE(batch.rebuildCount() == 1);
}

TEST_CASE("content is clipped to its area, and scrolling shows another part of it", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch batch;
    {
        const auto layers = batch.rebuild();
        // The panel: white, 60 wide and 80 high. Its content area starts 20 below the top.
        render::appendBox(layers.frame.shapes(), FloatRect(0.f, 0.f, 60.f, 80.f), solid(white));
        // The content is higher than the area: 40 of red, then 40 of green, then 40 of blue.
        render::appendBox(layers.content.shapes(), FloatRect(0.f, 20.f, 60.f, 40.f), solid(red));
        render::appendBox(layers.content.shapes(), FloatRect(0.f, 60.f, 60.f, 40.f), solid(green));
        render::appendBox(layers.content.shapes(), FloatRect(0.f, 100.f, 60.f, 40.f), solid(blue));
    }
    batch.setPosition({ 50.f, 30.f });
    batch.setContentClip(FloatRect(0.f, 20.f, 60.f, 60.f)); // in the window: y from 50 to 110
    const std::array<const PanelBatch*, 1> panels = { &batch };

    const FrameStats stats = scene.draw(panels);

    REQUIRE(scene.at(80, 40) == white);  // the header: above the content area
    REQUIRE(scene.at(80, 60) == red);    // the top of the content
    REQUIRE(scene.at(80, 100) == green); // further down
    REQUIRE(scene.at(80, 120) == clear); // below the panel: the content does not spill out
    REQUIRE(stats.drawCalls == 2);       // frame and content

    // Scrolled by 60: red has left through the top, blue has come in at the bottom.
    batch.setScroll(60.f);
    scene.draw(panels);

    REQUIRE(scene.at(80, 40) == white); // content scrolled up does not cover the header
    REQUIRE(scene.at(80, 60) == green);
    REQUIRE(scene.at(80, 100) == blue);
    REQUIRE(scene.at(80, 120) == clear);
    REQUIRE(batch.rebuildCount() == 1); // scrolling rebuilt nothing

    // What comes after is not clipped any more.
    PanelBatch next;
    paintPlain(next, red);
    next.setPosition({ 130.f, 100.f });
    const std::array<const PanelBatch*, 2> both = { &batch, &next };
    scene.draw(both);
    REQUIRE(scene.at(150, 130) == red);
}

TEST_CASE("without a clip area, content is drawn where it is", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch batch;
    {
        const auto layers = batch.rebuild();
        render::appendBox(layers.frame.shapes(), FloatRect(0.f, 0.f, 60.f, 40.f), solid(white));
        render::appendBox(
            layers.content.shapes(), FloatRect(10.f, 10.f, 20.f, 60.f), solid(red)
        ); // taller than the panel
    }
    batch.setPosition({ 20.f, 20.f });
    const std::array<const PanelBatch*, 1> panels = { &batch };

    scene.draw(panels);

    REQUIRE(scene.at(40, 40) == red);
    REQUIRE(scene.at(40, 80) == red); // below the panel's frame, since nothing clips it
}

TEST_CASE("a hidden panel is not drawn", "[ui][renderer][display]") {
    Scene scene;
    PanelBatch shown;
    PanelBatch hidden;
    paintPlain(shown, red);
    paintPlain(hidden, green);
    shown.setPosition({ 10.f, 10.f });
    hidden.setPosition({ 120.f, 10.f });
    hidden.setVisible(false);
    const std::array<const PanelBatch*, 2> panels = { &shown, &hidden };

    const FrameStats stats = scene.draw(panels);

    REQUIRE(scene.at(40, 30) == red);
    REQUIRE(scene.at(150, 30) == clear);
    REQUIRE(stats.panelsDrawn == 1);
    REQUIRE(stats.drawCalls == 1);
}

TEST_CASE("text is handed to the text renderer, layer by layer, after that layer's shapes", "[ui][renderer][display]") {
    struct Recorder final : render::TextRenderer {
        std::vector<std::string> seen;
        std::vector<sf::Vector2f> origins;

        std::size_t draw(sf::RenderTarget&, const sf::RenderStates& states, const render::DrawList& layer) override {
            for (const render::TextRun& run : layer.texts()) {
                seen.push_back(run.text);
                origins.push_back(states.transform.transformPoint({ 0.f, 0.f }));
            }
            return 1;
        }
    };

    Scene scene;
    Recorder recorder;
    scene.renderer.setTextRenderer(&recorder);

    PanelBatch batch;
    {
        const auto layers = batch.rebuild();
        render::appendBox(layers.frame.shapes(), FloatRect(0.f, 0.f, 60.f, 40.f), solid(white));
        layers.frame.addText(FloatRect(0.f, 0.f, 60.f, 16.f), "title", solid(red), Align::Left, false);
        layers.content.addText(FloatRect(0.f, 20.f, 60.f, 16.f), "value", solid(red), Align::Left, false);
    }
    batch.setPosition({ 30.f, 20.f });
    batch.setScroll(5.f);
    const std::array<const PanelBatch*, 1> panels = { &batch };

    const FrameStats stats = scene.draw(panels);

    REQUIRE(recorder.seen == std::vector<std::string>{ "title", "value" });
    REQUIRE(recorder.origins[0] == sf::Vector2f(30.f, 20.f)); // the frame's place
    REQUIRE(recorder.origins[1] == sf::Vector2f(30.f, 15.f)); // the content's, scrolled
    REQUIRE(stats.drawCalls == 3);                            // the frame's shapes, and one per text layer
}
