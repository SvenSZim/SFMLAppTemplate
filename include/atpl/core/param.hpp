#pragma once

#include "atpl/core/revision.hpp"

#include <atomic>
#include <concepts>
#include <mutex>
#include <type_traits>
#include <utility>

namespace atpl {

/// What a `Param` can hold: any type that can be copied and compared for equality.
/// Numbers, `bool`, enums and `std::string` all qualify.
template <typename T>
concept ParamValue = std::copyable<T> && std::equality_comparable<T>;

namespace detail {

/// Whether a `Param<T>` works without a lock: small, trivially copyable types the platform can
/// read and write atomically.
template <typename T>
consteval bool isLockFreeParam() {
    if constexpr (std::is_trivially_copyable_v<T>) {
        return std::atomic<T>::is_always_lock_free; // only asked of types an atomic can hold
    } else {
        return false;
    }
}

template <typename T>
inline constexpr bool lockFreeParam = isLockFreeParam<T>();

} // namespace detail

/// A single value shared between threads, for example between a widget and the simulation.
///
/// Reads and writes like a plain `T`:
///
///     Param<float> speed = 5.f;
///     speed = 7.5f;                 // write
///     position += velocity * speed; // read
///
/// Thread safety: any thread may read or write at any time. Every read returns a value that was
/// written as a whole; there are no torn or half-written values. Small trivially copyable types
/// (numbers, `bool`, enums) are lock-free, others (`std::string`) are guarded by a lock.
///
/// A `Param` stays where it is: it cannot be copied or moved, because widgets and the simulation
/// refer to the same object. It must outlive everything bound to it.
///
/// Where a `T` is deduced rather than converted to, name the value explicitly:
/// `std::max(speed.get(), 1.f)`.
template <ParamValue T>
class Param {
public:
    /// Starts with a value-initialised `T` (0, `false`, empty string).
    Param();

    /// Starts with `initial`. Not explicit, so `Param<float> speed = 5.f;` works.
    Param(T initial);

    Param(const Param&) = delete;
    Param(Param&&) = delete;

    /// The current value.
    [[nodiscard]] T get() const;

    /// Replaces the value. The revision grows only if the new value differs from the old one.
    void set(T value);

    /// Same as `get()`.
    operator T() const;

    /// Same as `set(value)`.
    Param& operator=(T value);

    /// Copies the other parameter's value: `a = b` means `a.set(b.get())`.
    Param& operator=(const Param& other);

    /// Grows each time the value changes. A reader that sees the same revision as last time
    /// knows the value is unchanged. A change may show up in the revision a moment after it
    /// shows up in the value, never the other way round.
    [[nodiscard]] Revision revision() const;

    /// Whether this parameter works without a lock.
    static constexpr bool isLockFree = detail::lockFreeParam<T>;

private:
    struct Locked {
        mutable std::mutex mutex;
        T value{};
    };
    using Storage = std::conditional_t<isLockFree, std::atomic<T>, Locked>;

    Storage m_storage;
    std::atomic<Revision> m_revision{ 0 };
};

// The value is stored before the revision grows: a reader that sees the new revision also
// sees the new value.

template <ParamValue T>
Param<T>::Param() :
    Param(T{}) {}

template <ParamValue T>
Param<T>::Param(T initial) {
    if constexpr (isLockFree) {
        m_storage.store(initial, std::memory_order_relaxed);
    } else {
        m_storage.value = std::move(initial);
    }
}

template <ParamValue T>
T Param<T>::get() const {
    if constexpr (isLockFree) {
        return m_storage.load(std::memory_order_acquire);
    } else {
        const std::lock_guard lock(m_storage.mutex);
        return m_storage.value;
    }
}

template <ParamValue T>
void Param<T>::set(T value) {
    bool changed = false;
    if constexpr (isLockFree) {
        changed = !(m_storage.exchange(value, std::memory_order_acq_rel) == value);
    } else {
        const std::lock_guard lock(m_storage.mutex);
        if (!(m_storage.value == value)) {
            m_storage.value = std::move(value);
            changed = true;
        }
    }
    if (changed) {
        m_revision.fetch_add(1, std::memory_order_release);
    }
}

template <ParamValue T>
Param<T>::operator T() const {
    return get();
}

template <ParamValue T>
Param<T>& Param<T>::operator=(T value) {
    set(std::move(value));
    return *this;
}

template <ParamValue T>
Param<T>& Param<T>::operator=(const Param& other) {
    if (this != &other) {
        set(other.get());
    }
    return *this;
}

template <ParamValue T>
Revision Param<T>::revision() const {
    return m_revision.load(std::memory_order_acquire);
}

} // namespace atpl
