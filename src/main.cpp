#include "app/app.hpp"
#include "ui/core/widgets/widget_factory.hpp"

using ui::core::widgets::Widget;
using ui::core::widgets::WidgetSize;
using ui::core::container::InnerLayout;

int main()
{
    App app({
        .uiSetup = {
            .windowName = "SFML Showcase",
            .windowSize = {1280, 720},
            .containers = {
                {.title = "Controls", .innerLayout = InnerLayout::Vertical, .widgets = {
                    Widget::Button("Reset"),
                    Widget::Slider("Speed", 0.f, 10.f, 5.f),
                    Widget::Switch("Gravity"),
                    Widget::Dropdown("Mode", {"Normal", "Debug", "Wireframe"})
                }},
                {.title = "Stats", .innerLayout = InnerLayout::TwoColumn, .widgets = {
                    Widget::TextDisplay("FPS"),
                    Widget::ProgressBar("Load"),
                    Widget::Graph("Frame Times", WidgetSize::Large)
                }}
            }
        }
    });
    app.run();
    return 0;
}
