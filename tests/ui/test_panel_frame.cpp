#include "atpl/ui/setup.hpp"

#include "ui/render/draw_list.hpp"
#include "ui/widgets/panel_frame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace atpl;
using atpl::render::DrawList;

namespace {

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

} // namespace

TEST_CASE("a panel's frame is its background and its title in the header", "[ui][widgets][panel]") {
    const Theme theme;
    const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f });
    DrawList list;
    Painter painter(list, { 0.f, 0.f }, { 280.f, 200.f });

    widgets::paintPanelFrame(painter, "Controls", theme, {}, sizes);

    REQUIRE(hasColor(list.shapes(), theme.resolve(Panel::Background).color));
    REQUIRE(list.texts().size() == 1);
    const render::TextRun& title = list.texts().front();
    REQUIRE(title.text == "Controls");
    REQUIRE(title.color == theme.resolve(Panel::Title).color);
    REQUIRE(title.size == theme.resolve(Panel::Title).textSize);
    // In the header, with the panel's padding at both sides.
    REQUIRE(title.rect == FloatRect(sizes.padding.x, 0.f, 280.f - 2.f * sizes.padding.x, sizes.headerHeight));
}

TEST_CASE("a panel is painted in its own colours", "[ui][widgets][panel]") {
    const Theme theme = themes::colorful();
    const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f });
    const PanelColors colors{ .main1 = 2, .main2 = 3, .accent = 1 };
    DrawList list;
    Painter painter(list, { 0.f, 0.f }, { 280.f, 200.f });

    widgets::paintPanelFrame(painter, "Playback", theme, colors, sizes);

    REQUIRE(hasColor(list.shapes(), theme.palette.mains[2]));       // background: main1
    REQUIRE_FALSE(hasColor(list.shapes(), theme.palette.mains[0])); // not the default
}

TEST_CASE("a panel squeezed below its header's height keeps its title inside", "[ui][widgets][panel]") {
    const Theme theme;
    const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f });
    DrawList list;
    Painter painter(list, { 280.f, 20.f }, { 280.f, 20.f });

    widgets::paintPanelFrame(painter, "Controls", theme, {}, sizes);

    REQUIRE(list.texts().size() == 1);
    REQUIRE(list.texts().front().rect.height() == 20.f);
}
