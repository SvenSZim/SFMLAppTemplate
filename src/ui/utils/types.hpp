#ifndef TYPES
#define TYPES

#include <SFML/System.hpp>

namespace uiutils {

template <typename T>
class iRect {
public:
    iRect() = default;
    iRect(const iRect<T> &other) = default;

    virtual inline sf::Vector2<T> getSize() const = 0;
    virtual void setSize(const sf::Vector2<T> &new_size) = 0;
    virtual inline sf::Vector2<T> getPosition() const = 0;
    virtual void setPosition(const sf::Vector2<T> &new_position) = 0;

    [[nodiscard]]
    sf::FloatRect toSFMLRect() const {
        return sf::Rect<T>(getPosition(), getSize());
    }

    [[nodiscard]]
    inline T width() const { return getSize().x; }
    void setWidth(T new_width) { setSize(sf::Vector2<T>(new_width, height())); }
    [[nodiscard]]
    inline T height() const { return getSize().y; }
    void setHeight(T new_height) { setSize(sf::Vector2<T>(width(), new_height)); }

    [[nodiscard]]
    inline T left() const { return getPosition().x; }
    void setLeft(T new_left) { setPosition(sf::Vector2<T>(new_left, top())); }
    void stickLeft(T new_left) { setWidth(width() + left() - new_left); setLeft(new_left); }
    [[nodiscard]]
    inline T right() const { return getPosition().x + width(); }
    void setRight(T new_right) { setPosition(sf::Vector2<T>(new_right - width(), top())); }
    void stickRight(T new_right) { setWidth(width() - right() + new_right); setRight(new_right); }
    [[nodiscard]]
    inline T top() const { return getPosition().y; }
    void setTop(T new_top) { setPosition(sf::Vector2<T>(left(), new_top)); }
    void stickTop(T new_top) { setHeight(height() + top() - new_top); setTop(new_top); }
    [[nodiscard]]
    inline T bottom() const { return getPosition().y + height(); }
    void setBottom(T new_bottom) { setPosition(sf::Vector2<T>(left(), new_bottom - height())); }
    void stickBottom(T new_bottom) { setHeight(height() - bottom() + new_bottom); setBottom(new_bottom); }

    [[nodiscard]]
    inline sf::Vector2<T> topLeft() const { return getPosition(); }
    void setTopLeft(const sf::Vector2<T> &new_top_left) { setPosition(new_top_left); }
    void stickTopLeft(const sf::Vector2<T> &new_top_left) { setSize(getSize() + topLeft() - new_top_left); setTopLeft(new_top_left); }
    [[nodiscard]]
    inline sf::Vector2<T> topRight() const { return getPosition() + sf::Vector2<T>(width(), 0); }
    void setTopRight(const sf::Vector2<T> &new_top_right) { setPosition(new_top_right - sf::Vector2<T>(width(), 0)); }
    void stickTopRight(const sf::Vector2<T> &new_top_right) { setSize(getSize() + sf::Vector2<T>(new_top_right.x - right(), top() - new_top_right.y)); setTopRight(new_top_right); }
    [[nodiscard]]
    inline sf::Vector2<T> bottomLeft() const { return getPosition() + sf::Vector2<T>(0, height()); }
    void setBottomLeft(const sf::Vector2<T> &new_bottom_left) { setPosition(new_bottom_left - sf::Vector2<T>(0, height())); }
    void stickBottomLeft(const sf::Vector2<T> &new_bottom_left) { setSize(getSize() + sf::Vector2<T>(left() - new_bottom_left.x, new_bottom_left.y - bottom())); setBottomLeft(new_bottom_left); }
    [[nodiscard]]
    inline sf::Vector2<T> bottomRight() const { return getPosition() + getSize(); }
    void setBottomRight(const sf::Vector2<T> &new_bottom_right) { setPosition(new_bottom_right - getSize()); }
    void stickBottomRight(const sf::Vector2<T> &new_bottom_right) { setSize(getSize() + new_bottom_right - bottomRight()); setBottomRight(new_bottom_right); }

    [[nodiscard]]
    inline sf::Vector2<T> center() const { return getPosition() + sf::Vector2<T>(getSize().x * 0.5f, getSize().y * 0.5f); }
    void setCenter(const sf::Vector2<T> &new_center) { setPosition(new_center - sf::Vector2<T>(getSize().x * 0.5f, getSize().y * 0.5f)); }

    bool operator==(const iRect<T> &other) const {
        return getPosition() == other.getPosition() && getSize() == other.getSize();
    }
    bool operator!=(const iRect<T> &other) const {
        return getPosition() != other.getPosition() || getSize() != other.getSize();
    }

    void operator=(const iRect<T> &other) {
        setPosition(other.getPosition());
        setSize(other.getSize());
    }
};

template <typename T>
class Rect : public iRect<T> {
private:
    sf::Vector2<T> position;
    sf::Vector2<T> size;
public:
    Rect() = default;
    Rect(sf::Rect<T> rect) : position(rect.position), size(rect.size) {}
    Rect(const iRect<T> &other) : position(other.getPosition()), size(other.getSize()) {}
    Rect(sf::Vector2<T> position, sf::Vector2<T> size) : position(position), size(size) {}
    Rect(T left, T top, T width, T height) : position(sf::Vector2<T>(left, top)), size(sf::Vector2<T>(width, height)) {}

    [[nodiscard]]
    inline sf::Vector2<T> getSize() const override { return size; }
    void setSize(const sf::Vector2<T> &new_size) override { size = new_size; }
    [[nodiscard]]
    inline sf::Vector2<T> getPosition() const override { return position; }
    void setPosition(const sf::Vector2<T> &new_position) override { position = new_position; }

    [[nodiscard]]
    Rect<T> operator+(const iRect<T> &other) const {
        return Rect(position + other.getPosition(), size + other.getSize());
    }
    [[nodiscard]]
    Rect<T> operator-(const iRect<T> &other) const {
        return Rect(position - other.getPosition(), size - other.getSize());
    }
    [[nodiscard]]
    Rect<T> operator*(T scalar) const {
        return Rect(position * scalar, size * scalar);
    }
    [[nodiscard]]
    Rect<T> inset(T delta) const {
        return Rect(position + sf::Vector2<T>(delta, delta), size - sf::Vector2<T>(delta * 2, delta * 2));
    }
};

typedef Rect<float> Rectf;

}

#endif