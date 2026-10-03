#pragma once

#include "atpl/core/revision.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace atpl {

/// Hands the latest complete state from one thread to another without either waiting for the other.
///
/// Its main use is the simulation publishing what the main thread draws. The simulation may be
/// much faster or much slower than the display: the reader always gets the newest state that was
/// published as a whole, never a half-written one, and states it has no time for are skipped.
///
///     Snapshot<World> snapshot;
///
///     // simulation thread, every tick
///     World& next = snapshot.writeBuffer();
///     fill(next);                           // write the whole state
///     snapshot.publish();
///
///     // main thread, every frame
///     const World& world = snapshot.read(); // newest published state
///     draw(world);
///
/// Three instances of `T` exist for the lifetime of the object and are reused, so publishing and
/// reading copy nothing and allocate nothing; containers inside `T` keep their memory.
///
/// Thread safety: exactly one writer thread (`writeBuffer`, `publish`) and exactly one reader
/// thread (`read`, `hasNew`). `revision()` may be called from any thread. Neither side ever blocks.
template <typename T>
class Snapshot {
public:
    /// All buffers start as a default-constructed `T`.
    Snapshot();

    /// All buffers start as a copy of `initial`. Use this to size containers up front.
    explicit Snapshot(const T& initial);

    Snapshot(const Snapshot&) = delete;
    Snapshot(Snapshot&&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot& operator=(Snapshot&&) = delete;

    // Writer side

    /// The buffer to write the next state into.
    ///
    /// After a `publish` it holds an older state, not the one just published. Write the whole
    /// state every time; do not build on what is in the buffer.
    [[nodiscard]] T& writeBuffer();

    /// Makes the contents of the write buffer the latest state. A previously published state
    /// that was never read is dropped.
    void publish();

    /// Shorthand for small states: copies `state` into the write buffer and publishes it.
    void publish(const T& state);

    // Reader side

    /// Whether a state was published since the last `read`.
    [[nodiscard]] bool hasNew() const;

    /// The newest published state. The reference stays valid, and the state unchanged, until the
    /// next call to `read`. Before the first publish this is the initial state.
    [[nodiscard]] const T& read();

    // Either side

    /// Grows with every publish.
    [[nodiscard]] Revision revision() const;

private:
    // A triple buffer. The writer owns one buffer and the reader another; the third is the one
    // handed over. `m_shared` says which buffer that is, and whether it holds a state the reader
    // has not taken yet. Handing over is one atomic exchange on either side: nobody waits.
    static constexpr std::uint8_t indexMask = 0x3;
    static constexpr std::uint8_t newState = 0x4;

    std::array<T, 3> m_buffers;
    std::uint8_t m_write = 0;                ///< Only the writer touches it.
    std::uint8_t m_read = 1;                 ///< Only the reader touches it.
    std::atomic<std::uint8_t> m_shared{ 2 }; ///< The buffer in between, and `newState` if unread.
    std::atomic<Revision> m_revision{ 0 };
};

// The exchange is acquire-release on both sides: what the writer wrote into a buffer before
// publishing it is seen by the reader that takes it, and what the reader read from a buffer it
// gives back is finished before the writer writes into it again.

template <typename T>
Snapshot<T>::Snapshot() = default;

template <typename T>
Snapshot<T>::Snapshot(const T& initial) :
    m_buffers{ initial, initial, initial } {}

template <typename T>
T& Snapshot<T>::writeBuffer() {
    return m_buffers[m_write];
}

template <typename T>
void Snapshot<T>::publish() {
    const std::uint8_t previous =
        m_shared.exchange(static_cast<std::uint8_t>(m_write | newState), std::memory_order_acq_rel);
    m_write = previous & indexMask; // a state the reader never took is written over next
    m_revision.fetch_add(1, std::memory_order_release);
}

template <typename T>
void Snapshot<T>::publish(const T& state) {
    writeBuffer() = state;
    publish();
}

template <typename T>
bool Snapshot<T>::hasNew() const {
    return (m_shared.load(std::memory_order_acquire) & newState) != 0;
}

template <typename T>
const T& Snapshot<T>::read() {
    if (hasNew()) {
        const std::uint8_t previous = m_shared.exchange(m_read, std::memory_order_acq_rel);
        m_read = previous & indexMask;
    }
    return m_buffers[m_read];
}

template <typename T>
Revision Snapshot<T>::revision() const {
    return m_revision.load(std::memory_order_acquire);
}

} // namespace atpl
