#ifndef RENDERCACHE
#define RENDERCACHE

#include <vector>
#include <unordered_map>
#include <SFML/Graphics.hpp>

#include "../types.hpp"

namespace ui::core::renderer {

struct CachedText {
    sf::Text text;
    bool visible = true;
};

struct CachedContainer {
    std::vector<sf::VertexArray> renderData;
    std::vector<CachedText> texts;
};

struct CachedWidget {
    std::vector<sf::VertexArray> renderData;
    std::vector<CachedText> texts;
};

struct RenderCache {
    std::unordered_map<ContainerID, CachedContainer> containers;
    std::unordered_map<WidgetID, CachedWidget> widgets;

    void render(sf::RenderWindow& window);
};

}

#endif