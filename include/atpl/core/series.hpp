#pragma once

#include "atpl/core/revision.hpp"

#include <cstddef>
#include <span>

namespace atpl {

/// A running series of samples shared between threads: the data behind a graph.
///
/// Holds the newest `capacity` samples; pushing more drops the oldest. Typically the simulation
/// pushes one sample per tick and a graph widget reads the window it shows.
///
///     Series frameTimes(240);
///     frameTimes.push(dt * 1000.f);        // simulation thread
///
///     std::array<float, 240> window;       // main thread
///     const std::size_t count = frameTimes.read(window);
///
/// Thread safety: any thread may push, read or clear at any time.
/// The capacity is fixed at construction; nothing is allocated afterwards.
class Series {
public:
    /// A series that keeps the newest `capacity` samples.
    explicit Series(std::size_t capacity);

    Series(const Series&) = delete;
    Series(Series&&) = delete;
    Series& operator=(const Series&) = delete;
    Series& operator=(Series&&) = delete;

    /// The largest number of samples kept.
    [[nodiscard]] std::size_t capacity() const;

    /// The number of samples currently held, at most `capacity()`.
    [[nodiscard]] std::size_t size() const;

    /// Appends one sample. If the series is full, the oldest sample is dropped.
    void push(float sample);

    /// Appends several samples in order, as one change.
    void push(std::span<const float> samples);

    /// Removes all samples.
    void clear();

    /// Copies the newest samples into `out`, oldest first, and returns how many were copied:
    /// the smaller of `size()` and `out.size()`. If `out` is smaller than the series, the
    /// newest `out.size()` samples are copied.
    std::size_t read(std::span<float> out) const;

    /// Grows with every push and every clear.
    [[nodiscard]] Revision revision() const;
};

} // namespace atpl
