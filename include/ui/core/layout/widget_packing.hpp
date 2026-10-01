#ifndef WIDGET_PACKING
#define WIDGET_PACKING

#include <vector>
#include <algorithm>

#include "../../utils/rect.hpp"
#include "../types.hpp"

namespace ui::core::layout {

using ui::utils::FloatRect;

struct PackedWidget {
    WidgetID id;
    FloatRect rect;
};

struct PackingResult {
    std::vector<PackedWidget> placements;
    float contentHeight = 0.f;
    bool overflows = false;
};

struct WidgetSizing {
    float minHeight = 20.f;
    float maxHeight = 100.f;
    float dynamicHeight = 0.05f;
    float minWidth = 40.f;
    float maxWidth = 400.f;
    float dynamicWidth = 1.f;
};

struct PackItem {
    WidgetID id;
    float resolvedHeight;
};

inline float resolveSize(float min, float max, float dynamic, float refDim) {
    return std::clamp(refDim * dynamic, min, max);
}

inline PackingResult packWidgets(
    const std::vector<PackItem>& items,
    float containerWidth,
    uint8_t columnCount,
    float maxContainerHeight,
    float padding = 4.f,
    float spacing = 4.f
) {
    PackingResult result;
    if (items.empty() || columnCount == 0) return result;

    const float colWidth = (containerWidth - padding * 2.f - spacing * (columnCount - 1)) / columnCount;
    const float startX = padding;
    const float startY = padding;

    float totalHeight = 0.f;
    for (const auto& item : items) {
        totalHeight += item.resolvedHeight + spacing;
    }
    float initialGuess = std::max((totalHeight / columnCount), items[0].resolvedHeight + spacing);

    float heightLimit = initialGuess;
    const float maxIterations = 20;
    const float step = (maxContainerHeight - initialGuess) / maxIterations;

    for (int iteration = 0; iteration < maxIterations + 1; iteration++) {
        result.placements.clear();
        result.overflows = false;

        uint8_t currentCol = 0;
        float colY = startY;
        bool allFit = true;

        for (const auto& item : items) {
            if (colY + item.resolvedHeight > heightLimit + startY && currentCol < columnCount - 1) {
                currentCol++;
                colY = startY;
            }

            if (colY + item.resolvedHeight > heightLimit + startY && currentCol >= columnCount - 1) {
                if (heightLimit >= maxContainerHeight) {
                    result.overflows = true;
                    float x = startX + currentCol * (colWidth + spacing);
                    result.placements.push_back({item.id, FloatRect(x, colY, colWidth, item.resolvedHeight)});
                    colY += item.resolvedHeight + spacing;
                    continue;
                }
                allFit = false;
                break;
            }

            float x = startX + currentCol * (colWidth + spacing);
            result.placements.push_back({item.id, FloatRect(x, colY, colWidth, item.resolvedHeight)});
            colY += item.resolvedHeight + spacing;
        }

        if (allFit || result.placements.size() == items.size()) {
            float maxColHeight = 0.f;
            std::vector<float> colHeights(columnCount, startY);
            for (const auto& p : result.placements) {
                float bottom = p.rect.bottom() + spacing;
                for (uint8_t c = 0; c < columnCount; c++) {
                    float colX = startX + c * (colWidth + spacing);
                    if (std::abs(p.rect.left() - colX) < 1.f) {
                        colHeights[c] = bottom;
                        break;
                    }
                }
            }
            for (auto h : colHeights) {
                maxColHeight = std::max(maxColHeight, h);
            }
            result.contentHeight = maxColHeight + padding - spacing;
            return result;
        }

        heightLimit += step;
        if (heightLimit > maxContainerHeight) heightLimit = maxContainerHeight;
    }

    result.contentHeight = maxContainerHeight;
    result.overflows = true;
    return result;
}

}

#endif
