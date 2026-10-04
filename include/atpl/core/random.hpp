#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>
#include <utility>
#include <vector>

namespace atpl {

/// A small, fast random number generator that gives the same numbers on every platform.
///
///     Random random(42);                         // the same seed, the same numbers, every run
///     float speed = random.range(1.0f, 3.0f);    // in [1, 3)
///     int cell = random.range(0, 9);             // 0 to 9, both included
///     if (random.chance(0.1)) { ... }            // one time in ten
///     auto direction = random.unitVector<sf::Vector2f>();
///
/// It is PCG32: 16 bytes of state, one multiplication per number, and 2^63 independent streams
/// for each seed. Integers, `uniform`, `range`, `below`, `chance`, `angle`, `unitVector` and
/// `inDisc` are computed without the standard library's distributions, whose results differ
/// between standard libraries, so a seed gives the same numbers with every compiler; `normal`
/// uses `std::log`, whose last bit may differ. `Random` also meets the standard's generator
/// requirements, for `std::shuffle` or a standard distribution.
///
/// A default-constructed generator uses a fixed seed, so a program is reproducible unless it
/// asks for `fromEntropy()`.
///
/// Thread safety: none; a `Random` belongs to one thread at a time. For loops spread over a
/// `ThreadPool`, use one generator per part (`RandomStreams`); for quick use anywhere, `threadRandom()`.
class Random {
public:
    using result_type = std::uint32_t;

    static constexpr std::uint64_t defaultSeed = 0x853c49e6748fea9bULL;

    /// The generator for `defaultSeed`, stream 0.
    Random();

    /// The generator for `seed`. Different `stream`s of one seed give independent sequences.
    explicit Random(std::uint64_t seed, std::uint64_t stream = 0);

    /// A generator seeded from the operating system's entropy: different on every run.
    [[nodiscard]] static Random fromEntropy();

    /// Starts again as `Random(seed, stream)`.
    void seed(std::uint64_t seed, std::uint64_t stream = 0);

    /// The next 32 random bits.
    std::uint32_t next();

    /// The next 32 random bits (the standard's generator interface).
    result_type operator()() { return next(); }
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    /// A number in [0, 1): 24 random bits for `float`, 53 for `double`.
    template <std::floating_point T = float>
    T uniform();

    /// A number in [min, max).
    template <std::floating_point T>
    T range(T min, T max);

    /// An integer from `min` to `max`, both included. Every value is equally likely.
    template <std::integral T>
    T range(T min, T max);

    /// An integer from 0 to `n - 1`. Every value is equally likely. `n` must be above 0.
    template <std::integral T>
    T below(T n);

    /// True with the probability `p` (0 never, 1 always).
    bool chance(double p);

    /// A normally distributed number.
    template <std::floating_point T = float>
    T normal(T mean = 0, T deviation = 1);

    /// An angle in radians in [0, 2 pi).
    float angle();

    /// A direction of length 1, every direction equally likely. `V` is any type made from `V{x, y}`.
    template <typename V = std::pair<float, float>>
    V unitVector();

    /// A point in the disc of `radius` around (0, 0), every point equally likely.
    template <typename V = std::pair<float, float>>
    V inDisc(float radius = 1.0f);

private:
    std::uint64_t next64();
    /// A point in the unit disc, by drawing in the square until one is inside.
    void pointInDisc(float& x, float& y);

    std::uint64_t m_state = 0;
    std::uint64_t m_increment = 1; ///< Odd; selects the stream.
};

/// One generator per part of a loop, all derived from one seed.
///
///     RandomStreams streams(seed, pool.maxParts());   // in the simulation, made once
///     pool.parallelFor(ants.size(), [&](std::size_t start, std::size_t end, std::size_t part) {
///         Random& random = streams[part];
///         for (std::size_t i = start; i < end; ++i) {
///             ants[i].wander(random);
///         }
///     });
///
/// The parts of a `ThreadPool` loop cover the same items on every run, so with a fixed seed and
/// part count the result is the same, whichever thread runs which part.
class RandomStreams {
public:
    /// `count` generators: stream `i` of `seed` is `Random(seed, i)`.
    RandomStreams(std::uint64_t seed, std::size_t count);

    [[nodiscard]] Random& operator[](std::size_t part) { return m_streams[part]; }
    [[nodiscard]] const Random& operator[](std::size_t part) const { return m_streams[part]; }
    [[nodiscard]] std::size_t size() const { return m_streams.size(); }

    /// Starts every stream again from `seed`.
    void seed(std::uint64_t seed);

private:
    std::vector<Random> m_streams;
};

/// A generator for the calling thread, seeded from entropy: for quick use where reproducing a
/// run does not matter. Safe from any thread, since every thread has its own.
[[nodiscard]] Random& threadRandom();

// Implementation

inline std::uint32_t Random::next() {
    // PCG-XSH-RR: a 64-bit linear congruential step, then a permutation of the old state.
    const std::uint64_t old = m_state;
    m_state = old * 6364136223846793005ULL + m_increment;
    const auto shifted = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
    const auto rotation = static_cast<std::uint32_t>(old >> 59U);
    return (shifted >> rotation) | (shifted << ((0U - rotation) & 31U));
}

inline std::uint64_t Random::next64() {
    const std::uint64_t high = next();
    return (high << 32U) | next();
}

template <std::floating_point T>
T Random::uniform() {
    if constexpr (sizeof(T) <= sizeof(float)) {
        return static_cast<T>(next() >> 8U) * static_cast<T>(0x1.0p-24);
    } else {
        return static_cast<T>(next64() >> 11U) * static_cast<T>(0x1.0p-53);
    }
}

template <std::floating_point T>
T Random::range(T min, T max) {
    const T value = min + uniform<T>() * (max - min);
    return value < max ? value : min; // rounding can reach max; it is outside the range
}

template <std::integral T>
T Random::range(T min, T max) {
    using U = std::make_unsigned_t<T>;
    const auto span = static_cast<std::uint64_t>(static_cast<U>(static_cast<U>(max) - static_cast<U>(min)));
    if (span == std::numeric_limits<std::uint64_t>::max()) {
        return static_cast<T>(next64()); // the whole range of a 64-bit type
    }
    const std::uint64_t count = span + 1;
    std::uint64_t offset = 0;
    // Rejecting the lowest values that would make some results more likely than others.
    if (count <= std::numeric_limits<std::uint32_t>::max()) {
        const auto n = static_cast<std::uint32_t>(count);
        const std::uint32_t threshold = (0U - n) % n;
        std::uint32_t bits = next();
        while (bits < threshold) {
            bits = next();
        }
        offset = bits % n;
    } else {
        const std::uint64_t threshold = (0ULL - count) % count;
        std::uint64_t bits = next64();
        while (bits < threshold) {
            bits = next64();
        }
        offset = bits % count;
    }
    return static_cast<T>(static_cast<U>(static_cast<U>(min) + static_cast<U>(offset)));
}

template <std::integral T>
T Random::below(T n) {
    return range<T>(T{ 0 }, static_cast<T>(n - 1));
}

inline bool Random::chance(double p) {
    return uniform<double>() < p;
}

template <std::floating_point T>
T Random::normal(T mean, T deviation) {
    // Box-Muller; 1 - uniform is in (0, 1], so the logarithm is finite.
    const double radius = std::sqrt(-2.0 * std::log(1.0 - uniform<double>()));
    const double turn = 2.0 * std::numbers::pi * uniform<double>();
    return mean + deviation * static_cast<T>(radius * std::cos(turn));
}

inline float Random::angle() {
    return range(0.0f, 2.0f * std::numbers::pi_v<float>);
}

inline void Random::pointInDisc(float& x, float& y) {
    do {
        x = range(-1.0f, 1.0f);
        y = range(-1.0f, 1.0f);
    } while (x * x + y * y >= 1.0f);
}

template <typename V>
V Random::unitVector() {
    float x = 0.0f;
    float y = 0.0f;
    float length2 = 0.0f;
    do {
        pointInDisc(x, y);
        length2 = x * x + y * y;
    } while (length2 < 1e-6f); // too close to the centre to give a precise direction
    const float length = std::sqrt(length2); // exactly rounded, so the same everywhere
    return V{ x / length, y / length };
}

template <typename V>
V Random::inDisc(float radius) {
    float x = 0.0f;
    float y = 0.0f;
    pointInDisc(x, y);
    return V{ x * radius, y * radius };
}

} // namespace atpl
