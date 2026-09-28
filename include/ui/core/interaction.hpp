#ifndef INTERACTION
#define INTERACTION

#include <vector>
#include <SFML/Graphics.hpp>
#include "./types.hpp"
#include "./container/container.hpp"

namespace ui::core {

struct InteractionState {
    ContainerID hoveredContainer = 0;
    ContainerID pressedContainer = 0;
    WidgetID hoveredWidget = 0;
    WidgetID pressedWidget = 0;
    sf::Vector2f mousePos = {0.f, 0.f};
};

inline ContainerID hitTestContainers(
    sf::Vector2f mousePos,
    const std::vector<container::Container>& containers
) {
    for (auto it = containers.rbegin(); it != containers.rend(); ++it) {
        if (it->isDisabled()) continue;
        const auto rect = it->rect.rect();
        if (mousePos.x >= rect.left() && mousePos.x <= rect.right() &&
            mousePos.y >= rect.top() && mousePos.y <= rect.bottom()) {
            return it->id;
        }
    }
    return 0;
}

}

#endif
