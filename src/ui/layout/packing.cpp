#include "ui/layout/packing.hpp"

namespace atpl::layout {

PackingResult packWidgets(
    std::span<const PackItem> items,
    float panelWidth,
    int columnCount,
    float maxPanelHeight,
    sf::Vector2f padding,
    sf::Vector2f spacing
) {
    PackingResult result;
    if (items.empty() || columnCount <= 0) {
        return result;
    }

    const auto columns = static_cast<float>(columnCount);
    const int lastColumn = columnCount - 1;
    const float colWidth = (panelWidth - padding.x * 2.f - spacing.x * (columns - 1.f)) / columns;
    const float startX = padding.x;
    const float startY = padding.y;

    float totalHeight = 0.f;
    for (const auto& item : items) {
        totalHeight += item.resolvedHeight + spacing.y;
    }
    // Start from an even split over the columns, but never above the maximum height:
    // otherwise everything "fits" into a column that is higher than the panel may be.
    const float evenSplit = std::max(totalHeight / columns, items[0].resolvedHeight + spacing.y);
    const float initialLimit = std::min(evenSplit, maxPanelHeight);

    constexpr int maxIterations = 20;
    const float step = (maxPanelHeight - initialLimit) / static_cast<float>(maxIterations);
    float heightLimit = initialLimit;

    // Bottom of each column including the spacing below its last item.
    std::vector<float> colHeights(static_cast<std::size_t>(columnCount));

    for (int iteration = 0; iteration <= maxIterations; ++iteration) {
        result.placements.clear();
        result.overflows = false;
        std::fill(colHeights.begin(), colHeights.end(), startY);

        int currentCol = 0;
        float colY = startY;
        bool allFit = true;

        const auto place = [&](const PackItem& item) {
            const float x = startX + static_cast<float>(currentCol) * (colWidth + spacing.x);
            result.placements.push_back({ item.id, FloatRect(x, colY, colWidth, item.resolvedHeight) });
            colY += item.resolvedHeight + spacing.y;
            colHeights[static_cast<std::size_t>(currentCol)] = colY;
        };

        for (const auto& item : items) {
            if (colY + item.resolvedHeight > heightLimit + startY && currentCol < lastColumn) {
                currentCol++;
                colY = startY;
            }

            if (colY + item.resolvedHeight > heightLimit + startY && currentCol >= lastColumn) {
                if (heightLimit < maxPanelHeight) {
                    // Does not fit yet: try again with higher columns.
                    allFit = false;
                    break;
                }
                // Already at the maximum height: let the last column run over.
                result.overflows = true;
            }

            place(item);
        }

        if (allFit) {
            const float maxColHeight = *std::max_element(colHeights.begin(), colHeights.end());
            result.contentHeight = maxColHeight + padding.y - spacing.y;
            return result;
        }

        // The last attempt uses exactly the maximum height, where everything is placed for certain.
        const bool lastAttemptNext = iteration + 1 == maxIterations;
        heightLimit = lastAttemptNext ? maxPanelHeight : std::min(heightLimit + step, maxPanelHeight);
    }

    return result; // not reached: the attempt at the maximum height always returns above
}

} // namespace atpl::layout
