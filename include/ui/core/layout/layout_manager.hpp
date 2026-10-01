#ifndef LAYOUTMANAGER
#define LAYOUTMANAGER

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../../utils/rect.hpp"
#include "../types.hpp"
#include "../container/container.hpp"
#include "./layoutstyle.hpp"
#include "./widget_packing.hpp"

namespace ui::core::layout {

using ui::utils::FloatRect;
using ui::core::container::Container;
using ui::core::container::ContainerState;

enum class OverflowMode : uint8_t {
    PriorityHiding      = 0,
    SpacingThenScroll   = 1
};

struct ContainerSizing {
    float minWidth = 40.f;
    float maxWidth = 200.f;
    float dynamicWidth = 0.12f;
    float minHeight = 20.f;
    float maxHeight = 400.f;
    float dynamicHeight = 0.5f;
    float collapsedHeightFraction = 0.04f;
};

struct GridConfig {
    uint8_t columnCount = 3;
    uint8_t appContainerSpan = 2;
};

class LayoutManager {
private:
    LayoutStyle m_style;
    OverflowMode m_overflowMode = OverflowMode::SpacingThenScroll;
    ContainerSizing m_sizing;
    GridConfig m_gridConfig;
    FloatRect m_screen;
    float m_scrollOffset = 0.f;

    void generateFloatingLayout(std::vector<Container>& containers);
    void generateGridLayout(std::vector<Container>& containers);

    float resolveContainerWidth() const;
    float resolveCollapsedHeight() const;
    float resolveMaxExpandedHeight() const;

public:
    LayoutManager() : m_style(LayoutStyle::Floating) {}
    LayoutManager(LayoutStyle style) : m_style(style) {}

    void setOverflowMode(OverflowMode mode) { m_overflowMode = mode; }
    [[nodiscard]] OverflowMode getOverflowMode() const { return m_overflowMode; }

    void setSizing(const ContainerSizing& sizing) { m_sizing = sizing; }
    void setGridConfig(const GridConfig& config) { m_gridConfig = config; }

    void generateLayout(FloatRect screen, std::vector<Container>& containers);
    void onResize(FloatRect newScreen, std::vector<Container>& containers);
    void update(std::vector<Container>& containers);
    void updateContainer(Container& container);
};

}

#endif
