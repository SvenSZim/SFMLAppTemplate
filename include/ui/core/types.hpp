#ifndef TYPES
#define TYPES

#include <cstdint>

namespace ui::core {

using ContainerID = uint8_t;
using WidgetID = uint16_t;
using StyleID = uint16_t;
using LayoutID = uint8_t;

typedef union DirtyFlag__ {
    uint8_t value;
    struct bits__ {
        uint8_t error : 1;
        uint8_t layout : 1;
        uint8_t visual : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
    } flags;
} DirtyFlag;

}

#endif