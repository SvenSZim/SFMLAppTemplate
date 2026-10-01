#pragma once

#include "atpl/core/revision.hpp"

#include <concepts>

namespace atpl {

/// What a `Param` can hold: any type that can be copied and compared for equality.
/// Numbers, `bool`, enums and `std::string` all qualify.
template <typename T>
concept ParamValue = std::copyable<T> && std::equality_comparable<T>;

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
};

} // namespace atpl
