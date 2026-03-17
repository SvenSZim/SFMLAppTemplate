#ifndef ANIMTYPES
#define ANIMTYPES

#include <SFML/System.hpp>

#include "./interpolated.hpp"
#include "./types.hpp"

namespace animutils {

template <typename T>
class Rect : public uiutils::iRect<T> {
private:
    Interpolated<sf::Vector2<T>> position;
    Interpolated<sf::Vector2<T>> size;
public:
    Rect() = default;
    Rect(sf::Rect<T> rect) : position(rect.position), size(rect.size) {}
    Rect(const Rect<T> &other) : position(other.position), size(other.size) {}
    Rect(const uiutils::iRect<T> &other) : position(other.getPosition()), size(other.getSize()) {}
    Rect(sf::Vector2<T> position, sf::Vector2<T> size) : position(position), size(size) {}
    Rect(T left = 0, T top = 0, T width = 0, T height = 0) : position(sf::Vector2<T>(left, top)), size(sf::Vector2<T>(width, height)) {}

    [[nodiscard]]
    inline sf::Vector2<T> getSize() const override { return size.getValue(); }
    [[nodiscard]]
    inline sf::Vector2<T> getSizeFinal() const { return size.end; }
    void setSize(const sf::Vector2<T> &new_size) override { size.setValue(new_size); }
    [[nodiscard]]
    inline sf::Vector2<T> getPosition() const override { return position.getValue(); }
    [[nodiscard]]
    inline sf::Vector2<T> getPositionFinal() const { return position.end; }
    void setPosition(const sf::Vector2<T> &new_position) override { position.setValue(new_position); }

    [[nodiscard]]
    uiutils::Rect<T> toRect() const {
        return uiutils::Rect<T>(position.getValue(), size.getValue());
    }

    void setTransition(TransitionFunction function) {
        position.setTransition(function);
        size.setTransition(function);
    }
    void setDuration(float duration) {
        position.setDuration(duration);
        size.setDuration(duration);
    }
    [[nodiscard]]
    bool transitionRunning() const {
        return position.running() || size.running();
    }

    [[nodiscard]]
    uiutils::Rect<T> operator+(const Rect<T> &other) const {
        return uiutils::Rect<T>(position.end + other.getPositionFinal(), size.end + other.getSizeFinal());
    }
    [[nodiscard]]
    uiutils::Rect<T> operator+(const uiutils::iRect<T> &other) const {
        return uiutils::Rect<T>(position.end + other.getPosition(), size.end + other.getSize());
    }
    [[nodiscard]]
    uiutils::Rect<T> operator-(const Rect<T> &other) const {
        return uiutils::Rect<T>(position.end - other.getPositionFinal(), size.end - other.getSizeFinal());
    }
    [[nodiscard]]
    uiutils::Rect<T> operator-(const uiutils::iRect<T> &other) const {
        return uiutils::Rect<T>(position.end - other.getPosition(), size.end - other.getSize());
    }
    [[nodiscard]]
    uiutils::Rect<T> operator*(T factor) const {
        return uiutils::Rect<T>(position.end * factor, size.end * factor);
    }
    [[nodiscard]]
    uiutils::Rect<T> inset(T delta) const {
        return uiutils::Rect<T>(position.end + sf::Vector2<T>(delta, delta), size.end - sf::Vector2<T>(delta, delta)*2);
    }
    [[nodiscard]]
    uiutils::Rect<T> dynamicInset(T delta) const {
        return uiutils::Rect<T>(position.getValue() + sf::Vector2<T>(delta, delta), size.getValue() - sf::Vector2<T>(delta * 2, delta * 2));
    }
};

typedef Rect<float> Rectf;

}

#endif