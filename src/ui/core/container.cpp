#include "./container.hpp"

#define _BORDER_INSET 0.5f

namespace ui {

Container::Container(const ContainerSetup &setup) :
    animRect(setup.rect),
    transitionRunning(false),
    outerRect(setup.rect),
    outlineThickness(setup.outlineThickness),
    innerRect(uiutils::Rectf(setup.rect).inset(setup.outlineThickness)),
    renderData({
        .cornerRadius = setup.cornerRadius,
        .cornerCount = setup.cornerCount,
        .outlineBorder = setup.outlineBorder,
        .fillColor = setup.fillColor,
        .outlineColorOuter = setup.outlineColorOuter,
        .outlineColorInner = setup.outlineColorInner,
        .borderColor = setup.borderColor
    })
{
    animRect.setTransition(animutils::TransitionFunction::EaseInOutExponential);
    animRect.setDuration(0.2f);
}

std::vector<sf::Vertex> generateRoundedRect(const uiutils::Rectf &rect, const int cornerRadius, const int cornerCount, const sf::Color &color) {
    std::vector<sf::Vertex> vertices;
    vertices.reserve(4 * (cornerCount + 2));

    const float cornerDeltaAngle = 0.5f * M_PI / (cornerCount + 1);
    const uiutils::Rectf innerRect = rect.inset(cornerRadius);

    // bottom-left -> top-left -> top-right -> bottom-right
    for (float angle = 2.f * M_PI; angle >= 1.5f * M_PI; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.bottomLeft() + offset, color});
    }
    for (float angle = 1.5f * M_PI; angle >= 1.f * M_PI; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.topLeft() + offset, color});
    }
    for (float angle = 1.f * M_PI; angle >= 0.5f * M_PI; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.topRight() + offset, color});
    }
    for (float angle = 0.5f * M_PI; angle >= 0.f * M_PI; angle -= cornerDeltaAngle) {
        const sf::Vector2f offset = sf::Vector2f(std::sin(angle) * cornerRadius, std::cos(angle) * cornerRadius);
        vertices.push_back({innerRect.bottomRight() + offset, color});
    }
    vertices.push_back({rect.bottomLeft() + sf::Vector2f(cornerRadius, 0.f), color});
    return vertices;
}

void Container::render(sf::RenderWindow &window) {
    // update transition state and get current rects
    if (transitionRunning && !animRect.transitionRunning()) transitionRunning = false;
    const uiutils::Rectf& outerRect = transitionRunning ? animRect.toRect() : this->outerRect;
    const uiutils::Rectf& innerRect = transitionRunning ? animRect.toRect().inset(outlineThickness) : this->innerRect;

    // render outline
    if (outlineThickness > 0) {
        sf::VertexArray outer(sf::PrimitiveType::TriangleStrip);
        if (renderData.cornerRadius > 0) {
            const auto verticesOuter = generateRoundedRect(outerRect, renderData.cornerRadius, renderData.cornerCount, renderData.outlineColorOuter);
            const auto verticesInner = generateRoundedRect(innerRect, std::max(0, renderData.cornerRadius - outlineThickness), renderData.cornerCount, renderData.outlineColorInner);
            for (size_t i{0}; i < verticesOuter.size() && i < verticesInner.size(); ++i) {
                outer.append(verticesOuter[i]);
                outer.append(verticesInner[i]);
            }
        } else {
            outer.append({innerRect.topLeft(), renderData.outlineColorOuter});
            outer.append({outerRect.topLeft(), renderData.outlineColorOuter});
            outer.append({innerRect.topRight(), renderData.outlineColorOuter});
            outer.append({outerRect.topRight(), renderData.outlineColorOuter});
            outer.append({innerRect.bottomRight(), renderData.outlineColorOuter});
            outer.append({outerRect.bottomRight(), renderData.outlineColorOuter});
            outer.append({innerRect.bottomLeft(), renderData.outlineColorOuter});
            outer.append({outerRect.bottomLeft(), renderData.outlineColorOuter});
            outer.append({innerRect.topLeft(), renderData.outlineColorOuter});
            outer.append({outerRect.topLeft(), renderData.outlineColorOuter});
        }
        window.draw(outer);

        if (renderData.outlineBorder) {
            sf::VertexArray border(sf::PrimitiveType::LineStrip);
            if (renderData.cornerRadius > 0) {
                const auto vertices = generateRoundedRect(outerRect, renderData.cornerRadius, renderData.cornerCount, renderData.borderColor);
                border.resize(vertices.size());
                for (size_t i{0}; i < vertices.size(); ++i) border[i] = vertices[i];
            } else {
                border.append({outerRect.topLeft(), renderData.borderColor});
                border.append({outerRect.topRight(), renderData.borderColor});
                border.append({outerRect.bottomRight(), renderData.borderColor});
                border.append({outerRect.bottomLeft(), renderData.borderColor});
                border.append({outerRect.topLeft(), renderData.borderColor});
            }
            window.draw(border);
        }
    }
    
    sf::VertexArray inner(sf::PrimitiveType::TriangleFan);
    if (renderData.cornerRadius > 0) {
        const auto vertices = generateRoundedRect(innerRect, std::max(0, renderData.cornerRadius - outlineThickness), renderData.cornerCount, renderData.fillColor);
        inner.resize(vertices.size());
        for (size_t i{0}; i < vertices.size(); ++i) inner[i] = vertices[i];
    } else {
        inner.append({innerRect.topLeft(), renderData.fillColor});
        inner.append({innerRect.topRight(), renderData.fillColor});
        inner.append({innerRect.bottomRight(), renderData.fillColor});
        inner.append({innerRect.bottomLeft(), renderData.fillColor});
    }
    window.draw(inner);

    if (renderData.outlineBorder) {
        sf::VertexArray border(sf::PrimitiveType::LineStrip);
        if (renderData.cornerRadius > 0) {
            const auto vertices = generateRoundedRect(innerRect, std::max(0, renderData.cornerRadius - outlineThickness), renderData.cornerCount, renderData.borderColor);
            border.resize(vertices.size());
            for (size_t i{0}; i < vertices.size(); ++i) border[i] = vertices[i];
        } else {
            border.append({innerRect.topLeft(), renderData.borderColor});
            border.append({innerRect.topRight(), renderData.borderColor});
            border.append({innerRect.bottomRight(), renderData.borderColor});
            border.append({innerRect.bottomLeft(), renderData.borderColor});
            border.append({innerRect.topLeft(), renderData.borderColor});
        }
        window.draw(border);
    }
}

}