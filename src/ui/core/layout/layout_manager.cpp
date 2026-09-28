#include "ui/core/layout/layout_manager.hpp"

namespace ui::core::layout {

float LayoutManager::resolveContainerWidth() const {
    const float screenDim = std::min(m_screen.width(), m_screen.height());
    return std::clamp(screenDim * m_sizing.dynamicWidth, m_sizing.minWidth, m_sizing.maxWidth);
}

float LayoutManager::resolveCollapsedHeight() const {
    const float screenH = m_screen.height();
    return std::clamp(screenH * m_sizing.collapsedHeightFraction, m_sizing.minHeight, m_sizing.minHeight * 2.f);
}

float LayoutManager::resolveMaxExpandedHeight() const {
    const float screenH = m_screen.height();
    return std::clamp(screenH * m_sizing.dynamicHeight, m_sizing.minHeight, m_sizing.maxHeight);
}

void LayoutManager::generateLayout(FloatRect screen, std::vector<Container>& containers) {
    m_screen = screen;
    switch (m_style) {
        case LayoutStyle::Floating:
            generateFloatingLayout(containers);
            break;
        case LayoutStyle::Grid:
            generateGridLayout(containers);
            break;
    }
}

void LayoutManager::onResize(FloatRect newScreen, std::vector<Container>& containers) {
    m_screen = newScreen;
    generateLayout(newScreen, containers);
}

void LayoutManager::generateFloatingLayout(std::vector<Container>& containers) {
    const int n = static_cast<int>(containers.size());
    if (n == 0) return;

    const float containerWidth = resolveContainerWidth();
    const float collapsedH = resolveCollapsedHeight();
    const float maxExpandedH = resolveMaxExpandedHeight();
    const float margin = std::min(m_screen.width(), m_screen.height()) * 0.03f;
    const float spacing = collapsedH * 0.5f;

    const int leftCount = (n + 1) / 2;
    const int rightCount = n / 2;

    float totalLeftHeight = leftCount * collapsedH + (leftCount - 1) * spacing;
    float totalRightHeight = rightCount > 0 ? rightCount * collapsedH + (rightCount - 1) * spacing : 0.f;
    const float availHeight = m_screen.height() - 2.f * margin;

    float leftSpacing = spacing;
    float rightSpacing = spacing;

    bool forceDisable = (collapsedH > availHeight);

    if (!forceDisable && m_overflowMode == OverflowMode::PriorityHiding) {
        int visibleLeft = leftCount;
        int visibleRight = rightCount;
        while (visibleLeft * collapsedH + (visibleLeft - 1) * leftSpacing > availHeight && visibleLeft > 1) {
            visibleLeft--;
        }
        while (visibleRight > 0 && visibleRight * collapsedH + (visibleRight - 1) * rightSpacing > availHeight) {
            visibleRight--;
        }
        int idx = 0;
        for (auto& cont : containers) {
            bool isLeft = idx < leftCount;
            int posInSide = isLeft ? idx : idx - leftCount;
            int limit = isLeft ? visibleLeft : visibleRight;
            if (posInSide >= limit) {
                cont.state.state.disabled = 1;
            } else {
                cont.state.state.disabled = 0;
            }
            idx++;
        }
    } else if (!forceDisable && m_overflowMode == OverflowMode::SpacingThenScroll) {
        if (totalLeftHeight > availHeight && leftCount > 1) {
            leftSpacing = std::max(2.f, (availHeight - leftCount * collapsedH) / (leftCount - 1));
        }
        if (totalRightHeight > availHeight && rightCount > 1) {
            rightSpacing = std::max(2.f, (availHeight - rightCount * collapsedH) / (rightCount - 1));
        }
    }

    if (forceDisable) {
        for (auto& cont : containers) {
            cont.state.state.disabled = 1;
        }
    }

    float leftX = m_screen.left() + margin;
    float rightX = m_screen.right() - margin - containerWidth;
    float leftY = m_screen.top() + margin;
    float rightY = m_screen.top() + margin;

    int idx = 0;
    for (auto& cont : containers) {
        if (cont.isDisabled()) {
            idx++;
            continue;
        }

        bool isLeft = idx < leftCount;
        float x = isLeft ? leftX : rightX;
        float& y = isLeft ? leftY : rightY;
        float curSpacing = isLeft ? leftSpacing : rightSpacing;

        FloatRect collapsed(x, y, containerWidth, collapsedH);
        FloatRect expanded(x, y, containerWidth, maxExpandedH);

        cont.collapsedRect = collapsed;
        cont.expandedRect = expanded;

        if (cont.isExpanded()) {
            cont.rect.setValue(expanded);
        } else {
            cont.rect.setValue(collapsed);
        }

        y += collapsedH + curSpacing;
        idx++;
    }
}

void LayoutManager::generateGridLayout(std::vector<Container>& containers) {
    const int n = static_cast<int>(containers.size());
    if (n == 0) return;

    const uint8_t cols = std::max(static_cast<uint8_t>(1), m_gridConfig.columnCount);
    const uint8_t appSpan = std::min(m_gridConfig.appContainerSpan, cols);
    const float margin = std::min(m_screen.width(), m_screen.height()) * 0.02f;
    const float spacing = margin * 0.5f;

    const float availWidth = m_screen.width() - 2.f * margin - spacing * (cols - 1);
    const float colWidth = availWidth / cols;
    const float availHeight = m_screen.height() - 2.f * margin;

    // All containers are permanently expanded in grid mode
    for (auto& cont : containers) {
        cont.state.state.expanded = 1;
        cont.state.state.disabled = 0;
    }

    // Pack non-app containers using iterative column-fill
    // App container gets the remaining space (spans appSpan columns)
    // For now: app container is implicit (not in the containers list)
    // UI containers fill the remaining columns

    uint8_t uiCols = cols - appSpan;
    if (uiCols == 0) uiCols = cols;

    float containerHeight = resolveMaxExpandedHeight();
    float uiColWidth = (uiCols > 0) ? (uiCols * colWidth + (uiCols - 1) * spacing) / uiCols : colWidth;

    // Place containers in grid using column-fill
    float startX = margin + appSpan * (colWidth + spacing);
    float startY = margin;

    std::vector<float> colHeights(uiCols, startY);
    int idx = 0;

    for (auto& cont : containers) {
        // Find shortest column
        uint8_t shortestCol = 0;
        float minH = colHeights[0];
        for (uint8_t c = 1; c < uiCols; c++) {
            if (colHeights[c] < minH) {
                minH = colHeights[c];
                shortestCol = c;
            }
        }

        float x = startX + shortestCol * (uiColWidth + spacing);
        float y = colHeights[shortestCol];

        float contH = std::min(containerHeight, availHeight - (y - margin));
        if (contH < m_sizing.minHeight) {
            cont.state.state.disabled = 1;
            idx++;
            continue;
        }

        FloatRect rect(x, y, uiColWidth, contH);
        cont.collapsedRect = rect;
        cont.expandedRect = rect;
        cont.rect.setValue(rect);

        colHeights[shortestCol] = y + contH + spacing;
        idx++;
    }
}

void LayoutManager::update(std::vector<Container>& containers) {
    for (auto& cont : containers) updateContainer(cont);
}

void LayoutManager::updateContainer(Container& container) {
    if (!container.flag.flags.layout) return;
    container.flag.flags.layout = 0;
}

}
