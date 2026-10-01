#pragma once

#include "atpl/core/revision.hpp"

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
};

} // namespace atpl
