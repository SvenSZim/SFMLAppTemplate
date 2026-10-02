#pragma once

#include <cstddef>

namespace atpl::render {

/// What drawing one frame took.
struct FrameStats {
    std::size_t drawCalls = 0;   ///< Calls that sent geometry to the graphics card, text included.
    std::size_t textCalls = 0;   ///< How many of those were for text.
    std::size_t triangles = 0;   ///< Triangles of shapes in those calls. Text is not counted.
    std::size_t panelsDrawn = 0; ///< Batches that were visible, the overlay included.
};

} // namespace atpl::render
