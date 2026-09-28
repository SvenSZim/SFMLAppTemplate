#include <algorithm>
#include <cmath>
#include "ui/core/renderer/widget_renderers.hpp"

namespace ui::core::renderer {

using ui::utils::FloatRect;

static const WidgetStateStyle& getStateStyle(const WidgetStyle& style, ButtonState state) {
    switch (state) {
        case ButtonState::Hovered: return style.hovered;
        case ButtonState::Pressed: return style.pressed;
        case ButtonState::Disabled: return style.disabled;
        default: return style.idle;
    }
}

static sf::VertexArray makeRect(const FloatRect& r, sf::Color color) {
    sf::VertexArray va(sf::PrimitiveType::TriangleFan, 4);
    va[0] = {r.topleft(), color};
    va[1] = {r.topright(), color};
    va[2] = {r.bottomright(), color};
    va[3] = {r.bottomleft(), color};
    return va;
}

static sf::VertexArray makeBorder(const FloatRect& r, sf::Color color) {
    sf::VertexArray va(sf::PrimitiveType::LineStrip, 5);
    va[0] = {r.topleft(), color};
    va[1] = {r.topright(), color};
    va[2] = {r.bottomright(), color};
    va[3] = {r.bottomleft(), color};
    va[4] = {r.topleft(), color};
    return va;
}

void renderButton(const ButtonWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const auto& ss = getStateStyle(style, widget.state);
    const FloatRect& r = widget.rect;

    cache.renderData.push_back(makeRect(r, ss.fill));
    cache.renderData.push_back(makeBorder(r, ss.border));

    if (text.isFontLoaded()) {
        sf::Text label = text.createText(widget.label, TextSize::Label, ss.text);
        auto bounds = label.getLocalBounds();
        label.setPosition({
            r.left() + (r.width() - bounds.size.x) * 0.5f,
            r.top() + (r.height() - bounds.size.y) * 0.5f - bounds.position.y
        });
        cache.texts.push_back({std::move(label), true});
    }
}

void renderSlider(const SliderWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const auto& ss = style.idle;
    const FloatRect& r = widget.rect;

    FloatRect trackRect(r.left(), r.top() + r.height() * 0.4f, r.width(), r.height() * 0.2f);
    cache.renderData.push_back(makeRect(trackRect, ss.border));

    float normalizedVal = (widget.max > widget.min) ? (widget.value - widget.min) / (widget.max - widget.min) : 0.f;
    float thumbX = r.left() + normalizedVal * (r.width() - 8.f);
    FloatRect thumbRect(thumbX, r.top(), 8.f, r.height());
    const auto& thumbStyle = widget.dragging ? style.pressed : style.idle;
    cache.renderData.push_back(makeRect(thumbRect, thumbStyle.fill));
    cache.renderData.push_back(makeBorder(thumbRect, thumbStyle.border));

    if (text.isFontLoaded() && !widget.name.empty()) {
        sf::Text label = text.createText(widget.name, TextSize::Small, ss.text);
        label.setPosition({r.left(), r.top() - 14.f});
        cache.texts.push_back({std::move(label), true});
    }
}

void renderTextInput(const TextInputWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const auto& ss = widget.focused ? style.hovered : style.idle;
    const FloatRect& r = widget.rect;

    cache.renderData.push_back(makeRect(r, ss.fill));
    cache.renderData.push_back(makeBorder(r, ss.border));

    if (text.isFontLoaded()) {
        sf::Text content = text.createText(widget.text.empty() ? widget.name : widget.text, TextSize::Label, widget.text.empty() ? sf::Color(128, 128, 128) : ss.text);
        content.setPosition({r.left() + 4.f, r.top() + 4.f});
        cache.texts.push_back({std::move(content), true});
    }
}

void renderDropdown(const DropdownWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const auto& ss = widget.open ? style.hovered : style.idle;
    const FloatRect& r = widget.rect;

    cache.renderData.push_back(makeRect(r, ss.fill));
    cache.renderData.push_back(makeBorder(r, ss.border));

    if (text.isFontLoaded()) {
        std::string displayText = widget.options.empty() ? widget.name : widget.options[widget.selectedIndex];
        sf::Text label = text.createText(displayText, TextSize::Label, ss.text);
        label.setPosition({r.left() + 4.f, r.top() + 4.f});
        cache.texts.push_back({std::move(label), true});
    }

    if (widget.open && !widget.options.empty()) {
        float optionH = r.height();
        float dropY = r.bottom();
        FloatRect dropBg(r.left(), dropY, r.width(), optionH * widget.options.size());
        cache.renderData.push_back(makeRect(dropBg, ss.fill));
        cache.renderData.push_back(makeBorder(dropBg, ss.border));

        if (text.isFontLoaded()) {
            for (size_t i = 0; i < widget.options.size(); i++) {
                sf::Color col = (i == widget.selectedIndex) ? style.pressed.text : ss.text;
                sf::Text opt = text.createText(widget.options[i], TextSize::Label, col);
                opt.setPosition({r.left() + 4.f, dropY + optionH * i + 4.f});
                cache.texts.push_back({std::move(opt), true});
            }
        }
    }
}

void renderSwitch(const SwitchWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const FloatRect& r = widget.rect;
    const auto& ss = widget.on ? style.pressed : style.idle;

    FloatRect trackRect(r.left(), r.top() + r.height() * 0.25f, r.width() * 0.4f, r.height() * 0.5f);
    cache.renderData.push_back(makeRect(trackRect, ss.border));

    float knobX = widget.on ? trackRect.right() - trackRect.height() : trackRect.left();
    FloatRect knobRect(knobX, trackRect.top(), trackRect.height(), trackRect.height());
    cache.renderData.push_back(makeRect(knobRect, ss.fill));

    if (text.isFontLoaded() && !widget.name.empty()) {
        sf::Text label = text.createText(widget.name, TextSize::Label, style.idle.text);
        label.setPosition({trackRect.right() + 8.f, r.top() + 2.f});
        cache.texts.push_back({std::move(label), true});
    }
}

void renderProgressBar(const ProgressBarWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const FloatRect& r = widget.rect;
    const auto& ss = style.idle;

    cache.renderData.push_back(makeRect(r, ss.fill));

    float fillWidth = r.width() * std::clamp(widget.value, 0.f, 1.f);
    if (fillWidth > 0.f) {
        FloatRect fillRect(r.left(), r.top(), fillWidth, r.height());
        cache.renderData.push_back(makeRect(fillRect, ss.border));
    }

    cache.renderData.push_back(makeBorder(r, ss.border));

    if (text.isFontLoaded() && !widget.name.empty()) {
        sf::Text label = text.createText(widget.name, TextSize::Small, ss.text);
        label.setPosition({r.left() + 4.f, r.top() + 2.f});
        cache.texts.push_back({std::move(label), true});
    }
}

void renderTextDisplay(const TextDisplayWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const FloatRect& r = widget.rect;
    const auto& ss = style.idle;

    if (text.isFontLoaded()) {
        if (!widget.name.empty()) {
            sf::Text label = text.createText(widget.name + ":", TextSize::Label, ss.text);
            label.setPosition({r.left(), r.top() + 2.f});
            cache.texts.push_back({std::move(label), true});
        }
        sf::Text value = text.createText(widget.text, TextSize::Value, ss.border);
        float valueX = widget.name.empty() ? r.left() : r.left() + r.width() * 0.5f;
        value.setPosition({valueX, r.top() + 2.f});
        cache.texts.push_back({std::move(value), true});
    }
}

void renderGraph(const GraphWidget& widget, const WidgetStyle& style, const TextRenderer& text, CachedWidget& cache) {
    cache.renderData.clear();
    cache.texts.clear();

    const FloatRect& r = widget.rect;
    const auto& ss = style.idle;

    cache.renderData.push_back(makeRect(r, ss.fill));
    cache.renderData.push_back(makeBorder(r, ss.border));

    if (!widget.data.empty() && widget.data.size() > 1) {
        float maxVal = *std::max_element(widget.data.begin(), widget.data.end());
        float minVal = *std::min_element(widget.data.begin(), widget.data.end());
        float range = maxVal - minVal;
        if (range < 0.001f) range = 1.f;

        size_t pointCount = std::min(widget.data.size(), static_cast<size_t>(r.width()));
        sf::VertexArray line(sf::PrimitiveType::LineStrip, pointCount);
        float step = r.width() / static_cast<float>(pointCount - 1);

        size_t startIdx = widget.data.size() > pointCount ? widget.data.size() - pointCount : 0;
        for (size_t i = 0; i < pointCount; i++) {
            float normalized = (widget.data[startIdx + i] - minVal) / range;
            float x = r.left() + i * step;
            float y = r.bottom() - normalized * (r.height() - 4.f) - 2.f;
            line[i] = {{x, y}, ss.border};
        }
        cache.renderData.push_back(line);
    }

    if (text.isFontLoaded() && !widget.name.empty()) {
        sf::Text label = text.createText(widget.name, TextSize::Small, ss.text);
        label.setPosition({r.left() + 2.f, r.top() + 2.f});
        cache.texts.push_back({std::move(label), true});
    }
}

}
