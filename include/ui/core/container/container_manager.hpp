#ifndef CONTAINERMANAGER
#define CONTAINERMANAGER

#include <string>
#include <vector>

#include "../../utils/rect.hpp"
#include "../types.hpp"
#include "../widgets/widget_factory.hpp"
#include "./container.hpp"

namespace ui::core::container {

using ui::utils::FloatRect;
using ui::core::widgets::WidgetDescriptor;

struct ContainerSetup {
    std::string title = "";
    FloatRect rect = {};
    InnerLayout innerLayout = InnerLayout::Vertical;
    std::vector<WidgetDescriptor> widgets = {};
    LayoutID layoutId = 0;
    StyleID styleId = 0;
};

class ContainerManager {
private:
    uint8_t m_container_count;
    std::vector<Container> m_containers;
public:
    ContainerManager();

    ContainerID createContainer(ContainerSetup&& setup);

    bool checkIntegrity();

    Container& getContainer(ContainerID id);
    std::vector<Container>& getAllContainer();
    [[nodiscard]] uint8_t getCount() const { return m_container_count; }

    void update();
};

}

#endif
