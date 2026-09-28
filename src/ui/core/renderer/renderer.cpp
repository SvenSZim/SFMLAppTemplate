#include "ui/core/renderer/renderer.hpp"

namespace ui::core::renderer {

// #################### HELPERS ####################

std::vector<sf::Vertex> generateRoundedRect(
    const ui::utils::FloatRect& rect,
    const float cornerRadius,
    const int cornerCount,
    const sf::Color& color
) {
    if (cornerRadius <= 0.f || cornerCount <= 0) {
        return {
            {rect.topleft(), color},
            {rect.topright(), color},
            {rect.bottomright(), color},
            {rect.bottomleft(), color},
            {rect.topleft(), color}
        };
    }
    constexpr float pi = 3.14159265359f;
    std::vector<sf::Vertex> vertices;
    vertices.reserve(4 * (cornerCount + 2));

    const float cornerDeltaAngle = 0.5f * pi / (cornerCount + 1);
    const ui::utils::FloatRect innerRect = rect.inset(cornerRadius);

    // bottom-left -> top-left -> top-right -> bottom-right
    for (float angle = 2.0f * pi; angle >= 1.5f * pi; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.bottomleft() + offset, color});
    }
    for (float angle = 1.5f * pi; angle >= 1.0f * pi; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.topleft() + offset, color});
    }
    for (float angle = 1.0f * pi; angle >= 0.5f * pi; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.topright() + offset, color});
    }
    for (float angle = 0.5f * pi; angle >= 0.0f * pi; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.bottomright() + offset, color});
    }
    vertices.push_back({rect.bottomleft() + sf::Vector2f(cornerRadius, 0.f), color});
    return vertices;
}

sf::VertexArray generateRoundedRectStrip(
    const ui::utils::FloatRect &outerRect,
    const ui::utils::FloatRect &innerRect,
    const float stripWidth,
    const float cornerRadius,
    const int cornerCount,
    const sf::Color &colorOuter,
    const sf::Color &colorInner
) {
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip);
    const auto verticesOuter = generateRoundedRect(outerRect, cornerRadius, cornerCount, colorOuter);
    const auto verticesInner = generateRoundedRect(innerRect, std::max(0.f, cornerRadius - stripWidth), cornerCount, colorInner);
    for (size_t i{0}; i < verticesOuter.size() && i < verticesInner.size(); ++i) {
        strip.append(verticesOuter[i]);
        strip.append(verticesInner[i]);
    }
    return strip;
}








// #################### Renderer ####################

Renderer::Renderer(RenderStyle&& style, const std::string& fontPath) : m_style(style) {
    m_text.loadFont(fontPath);
}

Renderer::Renderer(RenderStyles style, const std::string& fontPath) : m_style(getRenderStyle(style)) {
    m_text.loadFont(fontPath);
}

void Renderer::initStyles(
    std::vector<Container>& container
    // widgets
) {
    const int n_container_styles = m_style.containerStyles.size();
    const int n_container = container.size();
    int curstyleidx = 0;
    for (int contidx{0}; contidx < n_container; contidx++) {
        container[contidx].styleId = m_style.containerStyles[curstyleidx++].id;
        if (curstyleidx >= n_container_styles) curstyleidx = 0;
    }
}

void Renderer::switchStyle(
    RenderStyle&& style,
    std::vector<Container>& container
    //widgets
) {
    m_style = style;
    initStyles(container);
}

void Renderer::switchStyle(
    RenderStyles style,
    std::vector<Container>& container
    //widgets
) {
    switchStyle(getRenderStyle(style), container);
}

void Renderer::update(std::vector<Container>& container, WidgetManager& widgetMgr) {
    for (auto& cont : container) updateContainer(cont);
    updateWidgets(widgetMgr);
}

void Renderer::render(sf::RenderWindow& window) {
    renderEnvironment(window);
    m_cache.render(window);
    window.display();
}

// #################### helpers ####################

void Renderer::renderEnvironment(sf::RenderWindow& window) {
    window.clear(m_style.backgroundColor);
}

void Renderer::updateContainer(Container& container) {
    const ContainerID contID = container.id;
    std::unordered_map<ContainerID, CachedContainer>& cachedContainers = m_cache.containers; 
    if (cachedContainers.find(contID) != cachedContainers.end() && container.flag.flags.visual == 0) return;
    container.flag.flags.visual = 0; // clear flag
    cachedContainers[contID] = {}; // clear current render data

    // regenerate render data
    CachedContainer& renderData = cachedContainers[contID];
    const ContainerStyle& style = m_style.getContainerStyle(container.styleId);
    const ContainerStateStyle& stateStyle = m_style.getContainerStateStyle(style.stateStyles.size() < container.state.value ? style.stateStyles[0] : style.stateStyles[container.state.value]);
    const ui::utils::FloatRect outerRect = container.rect.rect();
    const ui::utils::FloatRect innerRect = outerRect.inset(style.outlineThickness);
    // render outline
    if (style.outlineThickness > 0) {
        const auto outline = generateRoundedRectStrip(
            outerRect,
            innerRect,
            style.outlineThickness,
            style.cornerRadius,
            m_style.roundedRectCornerCount,
            stateStyle.outlineColorOuter,
            stateStyle.outlineColorInner
        );
        renderData.renderData.push_back(outline);

        if (style.outlineBorder) {
            sf::VertexArray border(sf::PrimitiveType::LineStrip);
            const auto vertices = generateRoundedRect(
                outerRect,
                style.cornerRadius,
                m_style.roundedRectCornerCount,
                stateStyle.borderColor
            );
            border.resize(vertices.size());
            for (size_t i{0}; i < vertices.size(); ++i) border[i] = vertices[i];
            renderData.renderData.push_back(border);
        }
    }
    
    sf::VertexArray inner(sf::PrimitiveType::TriangleFan);
    const auto vertices = generateRoundedRect(
        innerRect,
        std::max(0.f, style.cornerRadius - style.outlineThickness),
        m_style.roundedRectCornerCount,
        stateStyle.fillColor
    );
    inner.resize(vertices.size());
    for (size_t i{0}; i < vertices.size(); ++i) inner[i] = vertices[i];
    renderData.renderData.push_back(inner);

    if (style.outlineBorder) {
        sf::VertexArray border(sf::PrimitiveType::LineStrip);
        const auto vertices = generateRoundedRect(
            innerRect,
            std::max(0.f, style.cornerRadius - style.outlineThickness),
            m_style.roundedRectCornerCount,
            stateStyle.borderColor
        );
        border.resize(vertices.size());
        for (size_t i{0}; i < vertices.size(); ++i) border[i] = vertices[i];
        renderData.renderData.push_back(border);
    }
}

void Renderer::updateWidgets(WidgetManager& widgetMgr) {
    for (auto& btn : widgetMgr.getButtons()) {
        if (m_cache.widgets.find(btn.id) != m_cache.widgets.end() && btn.flag.flags.visual == 0) continue;
        btn.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[btn.id];
        renderButton(btn, m_style.buttonStyle, m_text, cached);
    }
    for (auto& slider : widgetMgr.getSliders()) {
        if (m_cache.widgets.find(slider.id) != m_cache.widgets.end() && slider.flag.flags.visual == 0) continue;
        slider.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[slider.id];
        renderSlider(slider, m_style.sliderStyle, m_text, cached);
    }
    for (auto& input : widgetMgr.getTextInputs()) {
        if (m_cache.widgets.find(input.id) != m_cache.widgets.end() && input.flag.flags.visual == 0) continue;
        input.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[input.id];
        renderTextInput(input, m_style.textInputStyle, m_text, cached);
    }
    for (auto& dd : widgetMgr.getDropdowns()) {
        if (m_cache.widgets.find(dd.id) != m_cache.widgets.end() && dd.flag.flags.visual == 0) continue;
        dd.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[dd.id];
        renderDropdown(dd, m_style.dropdownStyle, m_text, cached);
    }
    for (auto& sw : widgetMgr.getSwitches()) {
        if (m_cache.widgets.find(sw.id) != m_cache.widgets.end() && sw.flag.flags.visual == 0) continue;
        sw.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[sw.id];
        renderSwitch(sw, m_style.switchStyle, m_text, cached);
    }
    for (auto& pb : widgetMgr.getProgressBars()) {
        if (m_cache.widgets.find(pb.id) != m_cache.widgets.end() && pb.flag.flags.visual == 0) continue;
        pb.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[pb.id];
        renderProgressBar(pb, m_style.progressBarStyle, m_text, cached);
    }
    for (auto& td : widgetMgr.getTextDisplays()) {
        if (m_cache.widgets.find(td.id) != m_cache.widgets.end() && td.flag.flags.visual == 0) continue;
        td.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[td.id];
        renderTextDisplay(td, m_style.textDisplayStyle, m_text, cached);
    }
    for (auto& graph : widgetMgr.getGraphs()) {
        if (m_cache.widgets.find(graph.id) != m_cache.widgets.end() && graph.flag.flags.visual == 0) continue;
        graph.flag.flags.visual = 0;
        auto& cached = m_cache.widgets[graph.id];
        renderGraph(graph, m_style.graphStyle, m_text, cached);
    }
}

}
