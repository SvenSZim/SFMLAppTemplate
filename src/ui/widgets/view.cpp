#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

namespace {

/// A region the application draws into. The widget itself draws nothing but an optional frame:
/// the UI calls the application's draw function at its place (ARCHITECTURE.md 4.7). Pointer
/// input over it goes to the application, not to the widget.
class ViewWidget final : public Widget {
public:
    explicit ViewWidget(ViewOptions options) :
        m_options(options) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const float least = sizes.rowHeight * 2.f;
        if (m_options.height > 0.f) {
            // A height of its own: fixed, scaled like everything else.
            const float height = std::max(m_options.height * sizes.scale.y, least);
            return { .min = { least, height },
                     .preferred = { context.width(), height },
                     .max = sf::Vector2f(widgets::anyWidth, height) };
        }
        // Otherwise as high as its width and shape say, and dynamic: in a grid cell it takes the
        // height the other widgets leave.
        const float aspect = m_options.aspectRatio > 0.f ? m_options.aspectRatio : 16.f / 9.f;
        return { .min = { least, least }, .preferred = { context.width(), std::max(context.width() / aspect, least) } };
    }

    void paint(Painter& painter, const Style& style) const override {
        painter.box(FloatRect({ 0.f, 0.f }, painter.size()), [&] {
            PartStyle frame = style.part(View::Frame); // hidden unless a theme shows it
            frame.border = frame.color;
            frame.borderThickness = std::max(frame.thickness, 1.f);
            frame.color = sf::Color::Transparent;
            return frame;
        }());
    }

private:
    ViewOptions m_options;
};

} // namespace

View::View(std::string viewName, ViewOptions viewOptions) :
    name(std::move(viewName)),
    options(viewOptions) {}

std::unique_ptr<Widget> View::create() const {
    return std::make_unique<ViewWidget>(options);
}

} // namespace atpl
