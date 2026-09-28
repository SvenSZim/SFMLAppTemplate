#ifndef RECT
#define RECT

#include <algorithm>
#include <type_traits>

#include <SFML/Graphics.hpp>

#include "./interpolated.hpp"

namespace ui::utils {

using ui::utils::anim::InterpolatedBase;
using ui::utils::anim::TransitionFunction;

template <class DerivedClass, typename T>
class iRect {
private:
    DerivedClass& self() {
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
        return static_cast<DerivedClass&>(*this);
    }

    const DerivedClass& self() const {
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
        return static_cast<const DerivedClass&>(*this);
    }

public:
    T width() const {
        return self().width();
    }

    void setWidth(T new_width) {
        self().setWidth(new_width);
    }

    T height() const {
        return self().height();
    }

    void setHeight(T new_height) {
        self().setHeight(new_height);
    }

    T left() const {
        return self().left();
    }

    void setLeft(T new_left) {
        self().setLeft(new_left);
    }

    void setLeftRescale(T new_left) {
        const T new_width = width() + left() - new_left;
        setWidth(new_width);
        setLeft(new_left);
    }

    T right() const {
        return left() + width();
    }

    void setRight(T new_right) {
        const T delta = new_right - right();
        setLeft(left() + delta);
    }

    void setRightRescale(T new_right) {
        const T new_width = width() - right() + new_right;
        setWidth(new_width);
        setRight(new_right);
    }

    T top() const {
        return self().top();
    }

    void setTop(T new_top) {
        self().setTop(new_top);
    }

    void setTopRescale(T new_top) {
        const T new_height = height() + top() - new_top;
        setHeight(new_height);
        setTop(new_top);
    }

    T bottom() const {
        return top() + height();
    }

    void setBottom(T new_bottom) {
        const T delta = new_bottom - bottom();
        setTop(top() + delta);
    }

    void setBottomRescale(T new_bottom) {
        const T new_height = height() - bottom() + new_bottom;
        setHeight(new_height);
        setBottom(new_bottom);
    }

    [[nodiscard]]
    sf::Vector2<T> size() const {
        return {width(), height()};
    }

    void setSize(const sf::Vector2<T>& new_size) {
        setWidth(new_size.x);
        setHeight(new_size.y);
    }

    [[nodiscard]]
    sf::Vector2<T> position() const {
        return {left(), top()};
    }

    void setPosition(const sf::Vector2<T>& new_position) {
        setLeft(new_position.x);
        setTop(new_position.y);
    }

    [[nodiscard]]
    sf::Rect<T> toSFMLRect() const {
        return {position(), size()};
    }

    [[nodiscard]]
    sf::Vector2<T> topleft() const {
        return {left(), top()};
    }

    void setTopleft(const sf::Vector2<T>& new_top_left) {
        setLeft(new_top_left.x);
        setTop(new_top_left.y);
    }

    void rescaleSetTopleft(const sf::Vector2<T>& new_top_left) {
        setSize(size() + topleft() - new_top_left);
        setTopleft(new_top_left);
    }

    [[nodiscard]]
    sf::Vector2<T> topright() const {
        return position() + sf::Vector2<T>(width(), 0);
    }

    void setTopright(const sf::Vector2<T>& new_top_right) {
        setRight(new_top_right.x);
        setTop(new_top_right.y);
    }

    void rescaleSetTopright(const sf::Vector2<T>& new_top_right) {
        setSize(size() + topright() - new_top_right);
        setTopright(new_top_right);
    }

    [[nodiscard]]
    sf::Vector2<T> bottomleft() const {
        return position() + sf::Vector2<T>(0, height());
    }

    void setBottomleft(const sf::Vector2<T>& new_bottom_left) {
        setLeft(new_bottom_left.x);
        setBottom(new_bottom_left.y);
    }

    void rescaleSetBottomleft(const sf::Vector2<T>& new_bottom_left) {
        setSize(size() + sf::Vector2<T>(left() - new_bottom_left.x, new_bottom_left.y - bottom()));
        setBottomleft(new_bottom_left);
    }

    [[nodiscard]]
    sf::Vector2<T> bottomright() const {
        return position() + size();
    }

    void setBottomright(const sf::Vector2<T>& new_bottom_right) {
        setRight(new_bottom_right.x);
        setBottom(new_bottom_right.y);
    }

    void rescaleSetBottomright(const sf::Vector2<T>& new_bottom_right) {
        setSize(size() + new_bottom_right - bottomright());
        setBottomright(new_bottom_right);
    }

    [[nodiscard]]
    sf::Vector2<T> center() const {
        return position() + sf::Vector2<T>(width() * 0.5f, height() * 0.5f);
    }

    void center(const sf::Vector2<T>& new_center) {
        setPosition(
            new_center - sf::Vector2<T>(width() * 0.5f, height() * 0.5f)
        );
    }

    bool operator==(const iRect<DerivedClass, T>& other) const {
        return left() == other.left() &&
            top() == other.top() &&
            width() == other.width() &&
            height() == other.height();
    }

    bool operator!=(const iRect<DerivedClass, T>& other) const {
        return !(*this == other);
    }

    void operator=(const iRect<DerivedClass, T>& other) {
        setWidth(other.width());
        setHeight(other.height());
        setLeft(other.left());
        setTop(other.top());
    }
};

template <typename T>
class Rect : public iRect<Rect<T>, T> {
private:
    T m_left{};
    T m_top{};
    T m_width{};
    T m_height{};

public:
    Rect() = default;

    Rect(sf::Rect<T> rect) :
        m_left(rect.position.x),
        m_top(rect.position.y),
        m_width(rect.size.x),
        m_height(rect.size.y)
    {}

    template <typename OtherRect>
    Rect(const iRect<OtherRect, T>& other) :
        m_left(other.left()),
        m_top(other.top()),
        m_width(other.width()),
        m_height(other.height())
    {}

    Rect(sf::Vector2<T> position, sf::Vector2<T> size) :
        m_left(position.x),
        m_top(position.y),
        m_width(size.x),
        m_height(size.y)
    {}

    Rect(T left, T top, T width, T height) :
        m_left(left),
        m_top(top),
        m_width(width),
        m_height(height)
    {}

    [[nodiscard]]
    T width() const {
        return m_width;
    }

    void setWidth(T new_width) {
        m_width = new_width;
    }

    [[nodiscard]]
    T height() const {
        return m_height;
    }

    void setHeight(T new_height) {
        m_height = new_height;
    }

    [[nodiscard]]
    T left() const {
        return m_left;
    }

    void setLeft(T new_left) {
        m_left = new_left;
    }

    [[nodiscard]]
    T top() const {
        return m_top;
    }

    void setTop(T new_top) {
        m_top = new_top;
    }

    template <typename OtherRect>
    [[nodiscard]]
    Rect operator+(const iRect<OtherRect, T>& other) const {
        return {
            m_left + other.left(),
            m_top + other.top(),
            m_width + other.width(),
            m_height + other.height()
        };
    }

    template <typename OtherRect>
    [[nodiscard]]
    Rect operator-(const iRect<OtherRect, T>& other) const {
        return {
            m_left - other.left(),
            m_top - other.top(),
            m_width - other.width(),
            m_height - other.height()
        };
    }

    [[nodiscard]]
    Rect operator*(T scalar) const {
        return {
            m_left * scalar,
            m_top * scalar,
            m_width * scalar,
            m_height * scalar
        };
    }

    [[nodiscard]]
    Rect<T> inset(T delta) const {
        return {
            m_left + delta,
            m_top + delta,
            m_width - delta * 2,
            m_height - delta * 2
        };
    }
};

using FloatRect = Rect<float>;
using IntRect = Rect<int>;

template <typename T>
class AnimRect : public iRect<AnimRect<T>, T>, public InterpolatedBase<AnimRect<T>, Rect<T>> {
private:
    T m_start_left{};
    T m_start_top{};
    T m_start_width{};
    T m_start_height{};
    T m_end_left{};
    T m_end_top{};
    T m_end_width{};
    T m_end_height{};
    float m_start_time_left{};
    float m_start_time_top{};
    float m_start_time_width{};
    float m_start_time_height{};

    [[nodiscard]]
    float status(float start_time) const {
        if (this->m_speed <= 0.0f) {
            return 1.0f;
        }
        return (this->getCurrentTime() - start_time) * this->m_speed;
    }

    [[nodiscard]]
    float alpha(float start_time) const {
        if (status(start_time) >= 1.0f) {
            return 1.0f;
        }
        return anim::getRatio(status(start_time), this->m_transition);
    }

public:
    AnimRect(TransitionFunction transition, float duration) :
        InterpolatedBase<AnimRect<T>, Rect<T>>(transition, duration)
    {
        const float initial_time =
            this->getCurrentTime() - anim::completedAnimationOffset(this->duration());
        m_start_time_left = initial_time;
        m_start_time_top = initial_time;
        m_start_time_width = initial_time;
        m_start_time_height = initial_time;
    }

    AnimRect(
        const sf::Rect<T>& rect,
        TransitionFunction transition = TransitionFunction::Linear,
        float duration = 1.0f
    ) :
        InterpolatedBase<AnimRect<T>, Rect<T>>(transition, duration),
        m_start_left(rect.position.x),
        m_start_top(rect.position.y),
        m_start_width(rect.size.x),
        m_start_height(rect.size.y),
        m_end_left(rect.position.x),
        m_end_top(rect.position.y),
        m_end_width(rect.size.x),
        m_end_height(rect.size.y)
    {
        const float initial_time =
            this->getCurrentTime() - anim::completedAnimationOffset(this->duration());
        m_start_time_left = initial_time;
        m_start_time_top = initial_time;
        m_start_time_width = initial_time;
        m_start_time_height = initial_time;
    }

    template <typename OtherRect>
    AnimRect(
        const iRect<OtherRect, T>& other,
        TransitionFunction transition = TransitionFunction::Linear,
        float duration = 1.0f
    ) :
        InterpolatedBase<AnimRect<T>, Rect<T>>(transition, duration),
        m_start_left(other.left()),
        m_start_top(other.top()),
        m_start_width(other.width()),
        m_start_height(other.height()),
        m_end_left(other.left()),
        m_end_top(other.top()),
        m_end_width(other.width()),
        m_end_height(other.height())
    {
        const float initial_time =
            this->getCurrentTime() - anim::completedAnimationOffset(this->duration());
        m_start_time_left = initial_time;
        m_start_time_top = initial_time;
        m_start_time_width = initial_time;
        m_start_time_height = initial_time;
    }

    AnimRect(
        sf::Vector2<T> position,
        sf::Vector2<T> size,
        TransitionFunction transition = TransitionFunction::Linear,
        float duration = 1.0f
    ) :
        InterpolatedBase<AnimRect<T>, Rect<T>>(transition, duration),
        m_start_left(position.x),
        m_start_top(position.y),
        m_start_width(size.x),
        m_start_height(size.y),
        m_end_left(position.x),
        m_end_top(position.y),
        m_end_width(size.x),
        m_end_height(size.y)
    {
        const float initial_time =
            this->getCurrentTime() - anim::completedAnimationOffset(this->duration());
        m_start_time_left = initial_time;
        m_start_time_top = initial_time;
        m_start_time_width = initial_time;
        m_start_time_height = initial_time;
    }

    AnimRect(
        T left = 0,
        T top = 0,
        T width = 0,
        T height = 0,
        TransitionFunction transition = TransitionFunction::Linear,
        float duration = 1.0f
    ) :
        InterpolatedBase<AnimRect<T>, Rect<T>>(transition, duration),
        m_start_left(left),
        m_start_top(top),
        m_start_width(width),
        m_start_height(height),
        m_end_left(left),
        m_end_top(top),
        m_end_width(width),
        m_end_height(height)
    {
        const float initial_time =
            this->getCurrentTime() - anim::completedAnimationOffset(this->duration());
        m_start_time_left = initial_time;
        m_start_time_top = initial_time;
        m_start_time_width = initial_time;
        m_start_time_height = initial_time;
    }

    [[nodiscard]]
    float status() const {
        const float max_start_time = std::max(
            {m_start_time_left, m_start_time_top, m_start_time_width, m_start_time_height}
        );
        return status(max_start_time);
    }

    [[nodiscard]]
    bool running() const {
        return status() < 1.0f;
    }

    [[nodiscard]]
    Rect<T> getValue() const {
        return rect();
    }

    void setValue(Rect<T> const& new_value) {
        m_start_left = left();
        m_start_top = top();
        m_start_width = width();
        m_start_height = height();
        m_end_left = new_value.left();
        m_end_top = new_value.top();
        m_end_width = new_value.width();
        m_end_height = new_value.height();

        const float start_time = this->getCurrentTime();
        m_start_time_left = start_time;
        m_start_time_top = start_time;
        m_start_time_width = start_time;
        m_start_time_height = start_time;
    }

    [[nodiscard]]
    T width() const {
        if (status(m_start_time_width) >= 1.0f) {
            return m_end_width;
        }
        return m_start_width + (m_end_width - m_start_width) * alpha(m_start_time_width);
    }

    void setWidth(T new_width) {
        m_start_width = width();
        m_end_width = new_width;
        m_start_time_width = this->getCurrentTime();
    }

    [[nodiscard]]
    T height() const {
        if (status(m_start_time_height) >= 1.0f) {
            return m_end_height;
        }
        return m_start_height + (m_end_height - m_start_height) * alpha(m_start_time_height);
    }

    void setHeight(T new_height) {
        m_start_height = height();
        m_end_height = new_height;
        m_start_time_height = this->getCurrentTime();
    }

    [[nodiscard]]
    T left() const {
        if (status(m_start_time_left) >= 1.0f) {
            return m_end_left;
        }
        return m_start_left + (m_end_left - m_start_left) * alpha(m_start_time_left);
    }

    void setLeft(T new_left) {
        m_start_left = left();
        m_end_left = new_left;
        m_start_time_left = this->getCurrentTime();
    }

    [[nodiscard]]
    T top() const {
        if (status(m_start_time_top) >= 1.0f) {
            return m_end_top;
        }
        return m_start_top + (m_end_top - m_start_top) * alpha(m_start_time_top);
    }

    void setTop(T new_top) {
        m_start_top = top();
        m_end_top = new_top;
        m_start_time_top = this->getCurrentTime();
    }

    [[nodiscard]]
    FloatRect rect() const {
        if (!running()) {
            return FloatRect(rectFinal());
        }
        return {left(), top(), width(), height()};
    }

    [[nodiscard]]
    Rect<T> rectFinal() const {
        return {m_end_left, m_end_top, m_end_width, m_end_height};
    }
};

using FloatAnimRect = AnimRect<float>;
using IntAnimRect = AnimRect<int>;

}

#endif
