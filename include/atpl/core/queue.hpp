#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace atpl {

/// A queue for handing items from one thread to another, in order.
///
/// Its main use is commands from the application (main thread) to the simulation: the
/// application pushes, the simulation takes everything that is waiting at the start of a tick.
///
///     Queue<Command> commands;
///     commands.push(Command::Reset);       // main thread
///
///     std::vector<Command> pending;        // simulation thread, kept between ticks
///     commands.drain(pending);
///     for (Command command : pending) { ... }
///     pending.clear();
///
/// Thread safety: any thread may call any member at any time. Items come out in the order
/// they went in. The queue has no size limit.
template <typename T>
class Queue {
public:
    Queue();

    Queue(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue& operator=(Queue&&) = delete;

    /// Adds an item at the end.
    void push(T item);

    /// Takes the oldest item, or nothing if the queue is empty. Never waits.
    [[nodiscard]] std::optional<T> tryPop();

    /// Moves every waiting item to the end of `out`, oldest first, and returns how many.
    /// Never waits. Takes all items in one step, so a tick sees a consistent batch.
    std::size_t drain(std::vector<T>& out);

    /// Like `drain`, but if the queue is empty, waits until an item arrives or `timeout` passes.
    /// Lets a paused simulation sleep until there is a command instead of polling.
    std::size_t waitDrain(std::vector<T>& out, std::chrono::milliseconds timeout);

    /// Removes every waiting item.
    void clear();

    /// The number of waiting items at the moment of the call. Another thread may change it at once.
    [[nodiscard]] std::size_t size() const;

    /// Whether nothing is waiting at the moment of the call.
    [[nodiscard]] bool empty() const;
};

} // namespace atpl
