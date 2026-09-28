#ifndef INTERPOLATED
#define INTERPOLATED

#include <chrono>

#include "./functions.hpp"

namespace ui::utils::anim {

inline float completedAnimationOffset(float duration_seconds) {
    return std::max(duration_seconds, 1.0f);
}

template <class DerivedClass, typename T>
class InterpolatedBase {
private:
    DerivedClass& self() {
        return static_cast<DerivedClass&>(*this);
    }

    const DerivedClass& self() const {
        return static_cast<const DerivedClass&>(*this);
    }

protected:
    float m_speed;
    TransitionFunction m_transition;

    [[nodiscard]]
    static float getCurrentTime() {
        const auto now = std::chrono::steady_clock::now();
        const auto duration = now.time_since_epoch();
        const auto seconds =
            std::chrono::duration_cast<std::chrono::duration<float>>(duration);
        return seconds.count();
    }

public:
    InterpolatedBase(TransitionFunction transition, float duration) :
        m_speed(duration > 0.0f ? 1.0f / duration : 0.0f),
        m_transition(transition)
    {}

    [[nodiscard]]
    float duration() const {
        return m_speed > 0.0f ? 1.0f / m_speed : 0.0f;
    }

    void setDuration(float duration) {
        m_speed = duration > 0.0f ? 1.0f / duration : 0.0f;
    }

    [[nodiscard]]
    TransitionFunction getTransition() const {
        return m_transition;
    }

    void setTransition(TransitionFunction function) {
        m_transition = function;
    }

    [[nodiscard]]
    T getValue() const {
        return self().getValue();
    }

    void setValue(T const& new_value) {
        self().setValue(new_value);
    }

    [[nodiscard]]
    operator T() const {
        return self().getValue();
    }

    void operator=(T const& new_value) {
        self().setValue(new_value);
    }
};

template <typename T>
class Interpolated : public InterpolatedBase<Interpolated<T>, T> {
private:
    T m_start;
    T m_end;
    float m_start_time;

    [[nodiscard]]
    float getElapsedSeconds() const {
        return this->getCurrentTime() - m_start_time;
    }

    [[nodiscard]]
    float alpha() const {
        if (!running()) {
            return 1.0f;
        }
        return getRatio(status(), this->m_transition);
    }

public:
    Interpolated(
        T const& initial_value = {},
        TransitionFunction transition = TransitionFunction::Linear,
        float duration = 1.0f
    ) :
        InterpolatedBase<Interpolated<T>, T>(transition, duration),
        m_start(initial_value),
        m_end(initial_value),
        m_start_time(this->getCurrentTime() - completedAnimationOffset(this->duration()))
    {}

    [[nodiscard]]
    float status() const {
        if (this->m_speed <= 0.0f) {
            return 1.0f;
        }
        return getElapsedSeconds() * this->m_speed;
    }

    [[nodiscard]]
    bool running() const {
        return status() < 1.0f;
    }

    [[nodiscard]]
    T getValue() const {
        if (!running()) {
            return m_end;
        }

        const T delta{m_end - m_start};
        return m_start + alpha() * delta;
    }

    void setValue(T const& new_value) {
        m_start = getValue();
        m_end = new_value;
        m_start_time = this->getCurrentTime();
    }
};

}

#endif
