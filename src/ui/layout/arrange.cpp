#include "ui/layout/arrange.hpp"

#include "atpl/ui/setup.hpp"

#include "ui/layout/panel_placement.hpp"
#include "ui/layout/rules.hpp"
#include "ui/layout/widget_layout.hpp"

#include <algorithm>
#include <cmath>
#include <variant>
#include <vector>

namespace atpl::layout {

namespace {

/// The most a floating panel may take of the window, per axis.
[[nodiscard]] sf::Vector2f limitOf(const PanelRules& rules, sf::Vector2f window, const Sizes& sizes) {
    return {
        std::floor(std::min(rules.limit.pixels.x * sizes.scale.x, rules.limit.windowFraction.x * window.x)),
        std::floor(std::min(rules.limit.pixels.y * sizes.scale.y, rules.limit.windowFraction.y * window.y)),
    };
}

/// How wide the panel's title is, with the padding at both sides.
[[nodiscard]] float
titleWidth(const model::Panel& panel, const Theme& theme, const Sizes& sizes, const render::TextMeasurer* measurer) {
    if (measurer == nullptr) {
        return 0.f;
    }
    const PartStyle style = theme.resolve(Panel::Title, State::Normal, panel.colors, sizes.text);
    return std::ceil(measurer->measure(panel.title, style.font, style.textSize).x) + sizes.padding.x * 2.f;
}

} // namespace

void prepare(model::Store& store, GridSetup grid, const Layout& layout) {
    preparePanels(store, grid);
    prepareWidgets(
        store, layout
    ); // after the panels: whether a panel is in the grid decides how its content is laid out
}

void arrange(
    model::Store& store,
    sf::Vector2f windowSize,
    GridSetup grid,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    const Layout& layout
) {
    const std::span<model::Panel> panels = store.panels();
    const std::size_t count = panels.size();
    const auto idOf = [](std::size_t index) { return PanelId{ static_cast<std::uint32_t>(index) }; };

    std::vector<PanelRules> rules(count);
    std::vector<float> widths(count);
    std::vector<bool> floating(count);
    std::vector<bool> sizedByContent(count); // floating, or fitting its content in the grid
    std::vector<bool> sharesCells(count);    // its grid's cells are as large as all others that share
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = panels[i];
        rules[i] = rulesFor(layout, panel);
        floating[i] = std::holds_alternative<Anchor>(panel.placement);
        sizedByContent[i] = floating[i] || rules[i].fit == Fit::Content;
        sharesCells[i] = sizedByContent[i] && panel.grid && rules[i].cells == SizeRule::Equal;
        panel.tooSmall = false;
        panel.wantedWidth = 0.f;
        panel.wantedHeight = 0.f;
    }

    // 0. Panels that share the size of their cells: the largest cell any of them would like.
    sf::Vector2f sharedCell;
    for (std::size_t i = 0; i < count; ++i) {
        if (sharesCells[i]) {
            const float width = panelWidth(panels[i], windowSize, grid, sizes);
            const WidgetLayout content =
                layoutWidgets(store, idOf(i), width, theme, sizes, measurer, std::nullopt, rules[i]);
            sharedCell = { std::max(sharedCell.x, content.cell.x), std::max(sharedCell.y, content.cell.y) };
        }
    }
    const auto cellFor = [&](std::size_t i) { return sharesCells[i] ? sharedCell : sf::Vector2f(); };

    // 1. Widths. A panel in the grid that fills its cells has theirs. Any other is as wide as its
    // content prefers, its title and the layout's panel width, or as wide as it says itself.
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = panels[i];
        widths[i] = panelWidth(panel, windowSize, grid, sizes);
        if (!sizedByContent[i]) {
            continue;
        }
        if (panel.width > 0.f) {
            panel.wantedWidth = std::round(panel.width * sizes.scale.x);
        } else {
            const WidgetLayout content =
                layoutWidgets(store, idOf(i), widths[i], theme, sizes, measurer, std::nullopt, rules[i], cellFor(i));
            panel.wantedWidth =
                std::max({ content.contentWidth, titleWidth(panel, theme, sizes, measurer), sizes.panelWidth });
        }
    }
    // Floating panels whose rule is `Equal` all get the widest of them; none goes beyond its limit.
    float widest = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        if (floating[i] && rules[i].width == SizeRule::Equal && panels[i].width <= 0.f) {
            widest = std::max(widest, panels[i].wantedWidth);
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = panels[i];
        if (floating[i] && rules[i].width == SizeRule::Equal && panel.width <= 0.f) {
            panel.wantedWidth = widest;
        }
        if (floating[i]) {
            panel.wantedWidth = std::min(panel.wantedWidth, limitOf(rules[i], windowSize, sizes).x);
        }
        if (sizedByContent[i]) {
            widths[i] = std::min(panel.wantedWidth, widths[i] > 0.f && !floating[i] ? widths[i] : panel.wantedWidth);
            panel.wantedWidth = widths[i];
        }
    }

    // 2. With the widths, the widgets at the sizes they prefer: the heights the panels would like.
    // `Equal` floating panels all get the highest; none goes beyond its limit.
    float highest = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = panels[i];
        layoutWidgets(store, idOf(i), widths[i], theme, sizes, measurer, std::nullopt, rules[i], cellFor(i));
        if (sizedByContent[i]) {
            panel.wantedHeight = sizes.headerHeight + panel.contentHeight;
        }
        if (floating[i] && rules[i].height == SizeRule::Equal && !panel.isClosed()) {
            highest = std::max(highest, panel.wantedHeight);
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = panels[i];
        if (floating[i] && rules[i].height == SizeRule::Equal) {
            panel.wantedHeight = highest;
        }
        if (floating[i]) {
            panel.wantedHeight = std::min(panel.wantedHeight, limitOf(rules[i], windowSize, sizes).y);
        }
    }

    // 3 and 4. Place the panels, and lay out their content for the room each actually got. A
    // panel too small for its widgets is left out and the others are placed again without it;
    // that can only leave more room for them, so this ends after a few rounds at most.
    for (std::size_t round = 0; round <= count; ++round) {
        placePanels(store, windowSize, grid, sizes, layout);

        bool leftOut = false;
        for (std::size_t i = 0; i < count; ++i) {
            model::Panel& panel = panels[i];
            if (!panel.shown || panel.isClosed()) {
                continue;
            }
            // While it folds or unfolds, the content keeps the place it has in the open panel.
            const float height = panel.opening.has_value() ? panel.openHeight : panel.rect.height();
            const sf::Vector2f content(panel.rect.width() - sizes.padding.x * 2.f, height - sizes.headerHeight);
            layoutWidgets(store, idOf(i), panel.rect.width(), theme, sizes, measurer, content.y, rules[i], cellFor(i));
            if (!panel.opening.has_value() &&
                (content.x < panel.widestAndHighest.x || content.y < panel.widestAndHighest.y)) {
                panel.tooSmall = true;
                leftOut = true;
            }
        }
        if (!leftOut) {
            break;
        }
    }

    // What is drawn: the widgets of open panels that fit their room.
    for (std::size_t i = 0; i < count; ++i) {
        const model::Panel& panel = panels[i];
        const bool open = panel.shown && !panel.isClosed();
        for (model::WidgetSlot& slot : store.widgetsOf(idOf(i))) {
            slot.visible = open && slot.fits;
        }
    }

    // 5. The views.
    placeViews(store, windowSize, sizes);
}

} // namespace atpl::layout
