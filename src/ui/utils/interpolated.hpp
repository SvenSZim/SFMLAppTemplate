#ifndef INTERPOLATED
#define INTERPOLATED


#include <chrono>
#include "./functions.hpp"

namespace animutils {

template<typename T>
struct Interpolated
{
  T start{};
  T end{};
  float start_time{};
  float speed{1.0f};
  TransitionFunction transition{TransitionFunction::Linear};

  explicit Interpolated(T const& initial_value = {}) :
    start{initial_value},
    end{start}
  {}

  [[nodiscard]]
  static float getCurrentTime() {
    auto const now = std::chrono::steady_clock::now();
    auto const duration = now.time_since_epoch();
    auto const seconds = std::chrono::duration_cast<std::chrono::duration<float>>(duration);
    return seconds.count();
  }

  [[nodiscard]]
  bool running() const {
    return getElapsedSeconds() * speed < 1.f;
  }

  [[nodiscard]]
  float getElapsedSeconds() const {
    return getCurrentTime() - start_time;
  }

  void setValue(T const& new_value) {
    start = getValue();
    end = new_value;
    start_time = getCurrentTime();
  }

  [[nodiscard]]
  T getValue() const {
    float const elapsed = getElapsedSeconds();
    float const t = elapsed * speed;
    if (t >= 1.0f) {
      return end;
    }
    T const delta{end - start};
    return start + delta * getRatio(t, transition);
  }

  void setDuration(float duration) {
    speed = 1.0f / duration;
  }
  
  void setTransition(TransitionFunction function) {
    if (getRatio(getElapsedSeconds() * speed, transition) > .8f) start = end;
    transition = function;
  }

  [[nodiscard]]
  operator T() const {
    return getValue();
  }

  void operator=(T const& new_value) {
    setValue(new_value);
  }
};

}

#endif