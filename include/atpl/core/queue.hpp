#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <optional>
#include <utility>
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

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_arrived; ///< Signalled with every push, for `waitDrain`.
    std::deque<T> m_items;
};

// One lock guards the items. Pushing wakes a waiting `waitDrain`; the item is in the queue
// before the waiter is woken.

template <typename T>
Queue<T>::Queue() = default;

template <typename T>
void Queue<T>::push(T item) {
    {
        const std::lock_guard lock(m_mutex);
        m_items.push_back(std::move(item));
    }
    m_arrived.notify_one();
}

template <typename T>
std::optional<T> Queue<T>::tryPop() {
    const std::lock_guard lock(m_mutex);
    if (m_items.empty()) {
        return std::nullopt;
    }
    std::optional<T> item(std::move(m_items.front()));
    m_items.pop_front();
    return item;
}

template <typename T>
std::size_t Queue<T>::drain(std::vector<T>& out) {
    const std::lock_guard lock(m_mutex);
    const std::size_t count = m_items.size();
    out.insert(out.end(), std::make_move_iterator(m_items.begin()), std::make_move_iterator(m_items.end()));
    m_items.clear();
    return count;
}

template <typename T>
std::size_t Queue<T>::waitDrain(std::vector<T>& out, std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_mutex);
    m_arrived.wait_for(lock, timeout, [this] { return !m_items.empty(); });
    const std::size_t count = m_items.size();
    out.insert(out.end(), std::make_move_iterator(m_items.begin()), std::make_move_iterator(m_items.end()));
    m_items.clear();
    return count;
}

template <typename T>
void Queue<T>::clear() {
    const std::lock_guard lock(m_mutex);
    m_items.clear();
}

template <typename T>
std::size_t Queue<T>::size() const {
    const std::lock_guard lock(m_mutex);
    return m_items.size();
}

template <typename T>
bool Queue<T>::empty() const {
    const std::lock_guard lock(m_mutex);
    return m_items.empty();
}

} // namespace atpl
