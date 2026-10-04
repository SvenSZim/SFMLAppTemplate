#include "atpl/core/random.hpp"

#include <chrono>
#include <random>

namespace atpl {

namespace {

/// Spreads the bits of a seed, so that nearby seeds (0, 1, 2, ...) start far apart (splitmix64).
std::uint64_t mix(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

} // namespace

Random::Random() :
    Random(defaultSeed) {}

Random::Random(std::uint64_t seed, std::uint64_t stream) {
    this->seed(seed, stream);
}

Random Random::fromEntropy() {
    std::random_device device;
    std::uint64_t seed = (static_cast<std::uint64_t>(device()) << 32U) | device();
    // Some platforms' random_device is deterministic; the clock makes runs differ there too.
    seed ^= static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t stream = (static_cast<std::uint64_t>(device()) << 32U) | device();
    return Random(seed, stream);
}

void Random::seed(std::uint64_t seed, std::uint64_t stream) {
    // The seeding of the PCG reference implementation, with the seed spread first.
    m_state = 0;
    m_increment = (stream << 1U) | 1U;
    next();
    m_state += mix(seed);
    next();
}

RandomStreams::RandomStreams(std::uint64_t seed, std::size_t count) :
    m_streams(count) {
    this->seed(seed);
}

void RandomStreams::seed(std::uint64_t seed) {
    for (std::size_t i = 0; i < m_streams.size(); ++i) {
        m_streams[i].seed(seed, i);
    }
}

Random& threadRandom() {
    thread_local Random generator = Random::fromEntropy();
    return generator;
}

} // namespace atpl
