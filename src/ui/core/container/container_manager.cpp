#include "ui/core/container/container_manager.hpp"

namespace ui::core::container {

ContainerManager::ContainerManager() :
    m_container_count(0),
    m_containers()
{}

bool ContainerManager::checkIntegrity() {
    bool corrupted = false;
    for (auto& container : m_containers) {
        if (container.rect.running() && container.flag.flags.visual == 0) {
            container.flag.flags.visual = 1;
            corrupted = true;
        }
    }
    return corrupted;
}

ContainerID ContainerManager::createContainer(ContainerSetup&& setup) {
    ContainerID id = ++m_container_count;
    Container cont{};
    cont.id = id;
    cont.title = std::move(setup.title);
    cont.rect = FloatAnimRect(
        setup.rect,
        ui::utils::anim::TransitionFunction::EaseInOutExponential,
        0.35f
    );
    cont.collapsedRect = setup.rect;
    cont.expandedRect = setup.rect;
    cont.innerLayout = setup.innerLayout;
    cont.layoutId = setup.layoutId;
    cont.styleId = setup.styleId;
    cont.firstWidget = setup.firstWidget;
    cont.widgetCount = setup.widgetCount;
    cont.state = {0};
    cont.flag = {0};
    m_containers.push_back(std::move(cont));
    return id;
}

Container& ContainerManager::getContainer(ContainerID id) {
    for (auto& cont : m_containers) {
        if (cont.id == id) return cont;
    }
    return m_containers[0];
}

std::vector<Container>& ContainerManager::getAllContainer() {
    return m_containers;
}

void ContainerManager::update() {
    for (auto& cont : m_containers) {
        cont.update();
    }
}

}
