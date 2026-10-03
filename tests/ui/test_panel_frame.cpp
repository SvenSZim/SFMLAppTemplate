#include "atpl/ui/setup.hpp"

#include "ui/layout/arrange.hpp"
#include "ui/layout/panel_placement.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/widgets/panel_frame.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>

using namespace atpl;
using atpl::render::DrawList;
using Catch::Approx;

namespace {

const float fold = Layout().foldSeconds;

const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f }); // nothing scaled

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

model::Panel panelNamed(std::string title, bool collapsible = true) {
    model::Panel panel;
    panel.name = title;
    panel.title = std::move(title);
    panel.collapsible = collapsible;
    return panel;
}

/// Paints the frame of a panel of 280 x 200 and returns what it drew.
DrawList paint(const model::Panel& panel, const Theme& theme = Theme(), sf::Vector2f size = { 280.f, 200.f }) {
    DrawList list;
    Painter painter(list, { 0.f, 0.f }, size);
    widgets::paintPanelFrame(painter, panel, theme, sizes);
    return list;
}

} // namespace

// ----- Painting -----

TEST_CASE("a panel's frame is its background and its title in the header", "[ui][widgets][panel]") {
    const Theme theme;
    const DrawList list = paint(panelNamed("Controls", false), theme);

    REQUIRE(hasColor(list.shapes(), theme.resolve(Panel::Background).color));
    REQUIRE(list.texts().size() == 1);
    const render::TextRun& title = list.texts().front();
    REQUIRE(title.text == "Controls");
    REQUIRE(title.color == theme.resolve(Panel::Title).color);
    REQUIRE(title.size == theme.resolve(Panel::Title).textSize);
    // In the header, with the panel's padding at both sides.
    REQUIRE(title.rect == FloatRect(sizes.padding.x, 0.f, 280.f - 2.f * sizes.padding.x, sizes.headerHeight));
}

TEST_CASE("a collapsible panel shows an arrow, and its title leaves room for it", "[ui][widgets][panel]") {
    Theme theme;
    theme[Panel::Arrow].color = sf::Color(1, 2, 3);
    const DrawList fixed = paint(panelNamed("Controls", false), theme);
    const DrawList foldable = paint(panelNamed("Controls", true), theme);

    REQUIRE_FALSE(hasColor(fixed.shapes(), sf::Color(1, 2, 3)));
    REQUIRE(hasColor(foldable.shapes(), sf::Color(1, 2, 3)));
    REQUIRE(foldable.texts().front().rect.width() == fixed.texts().front().rect.width() - sizes.headerHeight);
}

TEST_CASE("a theme can hide the arrow, and the title then has the whole header", "[ui][widgets][panel]") {
    Theme theme;
    theme[Panel::Arrow].color = sf::Color(1, 2, 3);
    theme[Panel::Arrow].shown = false;
    const DrawList list = paint(panelNamed("Controls", true), theme);

    REQUIRE_FALSE(hasColor(list.shapes(), sf::Color(1, 2, 3)));
    REQUIRE(list.texts().front().rect.width() == 280.f - 2.f * sizes.padding.x);
}

TEST_CASE("how long folding takes comes from the layout theme", "[ui][widgets][panel]") {
    UISetup setup;
    setup.panels = { { .name = "Controls" } };
    model::Store store{ setup };
    model::Panel& panel = store.panel(PanelId{ 0 });

    widgets::setCollapsed(panel, true);
    widgets::animatePanels(store, 0.1f, 0.4f); // a quarter of a slow fold
    REQUIRE(panel.openness() == Approx(0.75f));

    widgets::animatePanels(store, 0.f, 0.f); // no time at all: at once
    REQUIRE(panel.isClosed());
}

TEST_CASE("the arrow lights up while the pointer is over the header", "[ui][widgets][panel]") {
    const Theme theme = themes::colorful();
    model::Panel panel = panelNamed("Controls");
    const sf::Color idle = theme.resolve(Panel::Arrow).color;
    const sf::Color lit = theme.resolve(Panel::Title).color; // the title's colour
    REQUIRE(idle != lit);

    REQUIRE(hasColor(paint(panel, theme).shapes(), idle));
    panel.headerHovered = true;
    REQUIRE(hasColor(paint(panel, theme).shapes(), lit));
}

TEST_CASE("the header area and the line below it are shown only if the theme wants them", "[ui][widgets][panel]") {
    Theme theme;
    const sf::Color header(4, 5, 6);
    const sf::Color underline(7, 8, 9);
    theme[Panel::Header].color = header;
    theme[Panel::Underline].color = underline;

    const DrawList plain = paint(panelNamed("Controls"), theme);
    REQUIRE_FALSE(hasColor(plain.shapes(), header));
    REQUIRE_FALSE(hasColor(plain.shapes(), underline));

    theme[Panel::Header].shown = true;
    theme[Panel::Underline].shown = true;
    const DrawList shown = paint(panelNamed("Controls"), theme);
    REQUIRE(hasColor(shown.shapes(), header));
    REQUIRE(hasColor(shown.shapes(), underline));

    // A folded panel has nothing below its header to separate.
    model::Panel folded = panelNamed("Controls");
    folded.collapsed = true;
    REQUIRE_FALSE(hasColor(paint(folded, theme, { 280.f, sizes.headerHeight }).shapes(), underline));
}

TEST_CASE("a panel is painted in its own colours", "[ui][widgets][panel]") {
    const Theme theme = themes::colorful();
    model::Panel panel = panelNamed("Playback");
    panel.colors = { .main1 = 2, .main2 = 3, .accent = 1 };
    const DrawList list = paint(panel, theme);

    REQUIRE(hasColor(list.shapes(), theme.palette.mains[2]));       // background: main1
    REQUIRE_FALSE(hasColor(list.shapes(), theme.palette.mains[0])); // not the default
}

TEST_CASE("a panel squeezed below its header's height keeps its title inside", "[ui][widgets][panel]") {
    const DrawList list = paint(panelNamed("Controls"), Theme(), { 280.f, 20.f });
    REQUIRE(list.texts().size() == 1);
    REQUIRE(list.texts().front().rect.height() == 20.f);
}

// ----- Folding -----

TEST_CASE("folding a panel takes a moment, and turning round starts from where it is", "[ui][widgets][panel]") {
    model::Panel panel = panelNamed("Controls");
    REQUIRE(panel.openness() == 1.f);

    REQUIRE(widgets::setCollapsed(panel, true));
    REQUIRE(panel.collapsed);
    REQUIRE(panel.openness() == 1.f);                  // it starts open
    REQUIRE_FALSE(widgets::setCollapsed(panel, true)); // already on its way

    UISetup setup;
    setup.panels = { { .name = "Controls" } };
    model::Store store{ setup };
    model::Panel& stored = store.panel(PanelId{ 0 });
    widgets::setCollapsed(stored, true);

    REQUIRE(widgets::animatePanels(store, fold * 0.25f, fold));
    REQUIRE(stored.openness() == Approx(0.75f));
    REQUIRE_FALSE(stored.isClosed());

    widgets::setCollapsed(stored, false); // clicked again half-way: back from 0.75
    REQUIRE(stored.openness() == Approx(0.75f));
    widgets::animatePanels(store, fold * 0.1f, fold);
    REQUIRE(stored.openness() == Approx(0.85f));

    widgets::animatePanels(store, fold, fold); // more than enough
    REQUIRE(stored.openness() == 1.f);
    REQUIRE_FALSE(stored.opening.has_value());                // at rest
    REQUIRE_FALSE(widgets::animatePanels(store, 0.1f, fold)); // nothing moves any more
}

TEST_CASE("a folded panel is closed once it has arrived", "[ui][widgets][panel]") {
    UISetup setup;
    setup.panels = { { .name = "Controls" } };
    model::Store store{ setup };
    model::Panel& panel = store.panel(PanelId{ 0 });

    widgets::setCollapsed(panel, true);
    widgets::animatePanels(store, fold * 0.99f, fold);
    REQUIRE_FALSE(panel.isClosed());
    widgets::animatePanels(store, fold, fold);
    REQUIRE(panel.isClosed());
}

TEST_CASE("a folding panel shrinks gently, and the panels below follow", "[ui][widgets][panel]") {
    Sizes round = sizes;
    round.margin = 10.f;
    round.headerHeight = 30.f;
    model::Panel panel = panelNamed("Controls");
    panel.contentHeight = 100.f;

    panel.opening = 0.5f;
    REQUIRE(layout::openShare(panel) == Approx(0.5f));
    REQUIRE(layout::wantedHeight(panel, round) == 80.f); // half-way between 30 and 130

    panel.opening = 0.25f; // gentle at the ends: less than a quarter of the way
    REQUIRE(layout::openShare(panel) < 0.25f);
    panel.opening.reset();
    panel.collapsed = true;
    REQUIRE(layout::wantedHeight(panel, round) == 30.f);
}
