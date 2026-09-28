#ifndef WIDGET_RENDERERS
#define WIDGET_RENDERERS

#include <SFML/Graphics.hpp>

#include "../widgets/widget_types.hpp"
#include "./renderstyle.hpp"
#include "./rendercache.hpp"
#include "./text.hpp"
#include "../../utils/rect.hpp"

namespace ui::core::renderer {

using namespace ui::core::widgets;

void renderButton(const ButtonWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderSlider(const SliderWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderTextInput(const TextInputWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderDropdown(const DropdownWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderSwitch(const SwitchWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderProgressBar(const ProgressBarWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderTextDisplay(const TextDisplayWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);
void renderGraph(const GraphWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache);

}

#endif
