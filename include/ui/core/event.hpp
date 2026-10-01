#ifndef EVENT
#define EVENT

#include <cstdint>
#include "./types.hpp"

namespace ui::core {

struct Event {
    enum class Type : uint8_t {
        Closed = 0,
        WidgetChanged = 1
    };

    Type type;
    WidgetID source;

    Event(Type t, WidgetID src = 0) : type(t), source(src) {}

    static Event closed() { return {Type::Closed, 0}; }
    static Event widgetChanged(WidgetID id) { return {Type::WidgetChanged, id}; }
};

}

#endif
