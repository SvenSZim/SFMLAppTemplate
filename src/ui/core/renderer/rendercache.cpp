#include "ui/core/renderer/rendercache.hpp"

namespace ui::core::renderer {

void RenderCache::render(sf::RenderWindow& window) {
    for (auto& pair : containers) {
        for (auto& va : pair.second.renderData)
            window.draw(va);
        for (auto& ct : pair.second.texts) {
            if (ct.visible)
                window.draw(ct.text);
        }
    }

    for (auto& pair : widgets) {
        for (auto& va : pair.second.renderData)
            window.draw(va);
        for (auto& ct : pair.second.texts) {
            if (ct.visible)
                window.draw(ct.text);
        }
    }
}

}