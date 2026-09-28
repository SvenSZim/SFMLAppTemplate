#ifndef TYPES
#define TYPES

#include <cstdint>

namespace ui::core {

using ContainerID = uint8_t;
using WidgetID = uint16_t;
using StyleID = uint16_t;
using LayoutID = uint8_t;

typedef union DirtyFlag__ {
    char value;
    struct bits__ {
        char error : 1;
        char layout : 1;
        char visual : 1;
        char b3 : 1;
        char b4 : 1;
        char b5 : 1;
        char b6 : 1;
        char b7 : 1;
    } flags;
} DirtyFlag;

}

#endif