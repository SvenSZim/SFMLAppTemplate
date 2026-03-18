#ifndef CONTAINER
#define CONTAINER

#include <cmath>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>

#include "../utils/animtypes.hpp"

namespace ui {

struct ContainerSetup {
    const sf::FloatRect rect = {{100.0f, 100.0f}, {700.0f, 500.0f}};
    const int cornerCount = 16;
    const int cornerRadius = 80;
    const int outlineThickness = 50;
    const bool outlineBorder = false;
    const sf::Color fillColor = sf::Color(110, 110, 110, 255);
    const sf::Color outlineColorOuter = sf::Color::Transparent;
    const sf::Color outlineColorInner = sf::Color(95, 0, 160, 255);
    const sf::Color borderColor = sf::Color::Black;
};

struct ContainerRenderData {
    int cornerRadius;
    int cornerCount;
    bool outlineBorder;
    sf::Color fillColor;
    sf::Color outlineColorOuter;
    sf::Color outlineColorInner;
    sf::Color borderColor;
};

class Container : public uiutils::iRect<float> {
public:
    Container(const ContainerSetup &setup);
    ~Container() = default;

    void render(sf::RenderWindow &window);

    inline sf::Vector2f getSize() const override { return outerRect.getSize(); }
    void setSize(const sf::Vector2f &new_size) override {
        outerRect.setSize(new_size);
        innerRect = outerRect.inset(outlineThickness);
        animRect.setSize(new_size);
        transitionRunning = true;
    }
    inline sf::Vector2f getPosition() const override { return outerRect.getPosition(); }
    void setPosition(const sf::Vector2f &new_position) override {
        outerRect.setPosition(new_position);
        innerRect = outerRect.inset(outlineThickness);
        animRect.setPosition(new_position);
        transitionRunning = true;
    }

private:
    animutils::Rectf animRect;
    bool transitionRunning;
    uiutils::Rectf outerRect;
    int outlineThickness;
    uiutils::Rectf innerRect;

    ContainerRenderData renderData;
};

}

#endif