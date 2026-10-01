#ifndef RENDERSTYLETEMPLATES
#define RENDERSTYLETEMPLATES

#include "./renderstyle.hpp"

namespace ui::core::renderer {

typedef enum class RenderStyles__ {
    Moon = 0,
    Colorful = 1
    // etc.
}RenderStyles;

RenderStyle getRenderStyle(RenderStyles style);

};

#endif