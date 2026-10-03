#pragma once

#include "atpl/core/revision.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <span>
#include <vector>

namespace atpl::detail {

/// The newest `capacity` items of a running sequence, shared between threads: the storage behind
/// `Series` and `PointSeries`.
///
/// A lock guards the items. It is held only for a copy into or out of the buffer; the revision
/// can be read without it. The memory is allocated once, at construction.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) :
        m_items(capacity) {}

    [[nodiscard]] std::size_t capacity() const { return m_items.size(); }

    [[nodiscard]] std::size_t size() const {
        const std::lock_guard lock(m_mutex);
        return m_size;
    }

    void push(std::span<const T> items) {
        if (items.empty() || m_items.empty()) {
            return;
        }
        // More than fit: only the newest count.
        if (items.size() > m_items.size()) {
            items = items.last(m_items.size());
        }
        {
            const std::lock_guard lock(m_mutex);
            for (const T& item : items) {
                m_items[(m_start + m_size) % m_items.size()] = item;
                if (m_size < m_items.size()) {
                    ++m_size;
                } else {
                    m_start = (m_start + 1) % m_items.size();
                }
            }
        }
        m_revision.fetch_add(1, std::memory_order_release); // after the items, never before
    }

    void clear() {
        {
            const std::lock_guard lock(m_mutex);
            m_start = 0;
            m_size = 0;
        }
        m_revision.fetch_add(1, std::memory_order_release);
    }

    std::size_t read(std::span<T> out) const {
        const std::lock_guard lock(m_mutex);
        const std::size_t count = std::min(m_size, out.size());
        const std::size_t skipped = m_size - count; // the oldest that do not fit
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = m_items[(m_start + skipped + i) % m_items.size()];
        }
        return count;
    }

    [[nodiscard]] Revision revision() const { return m_revision.load(std::memory_order_acquire); }

private:
    mutable std::mutex m_mutex;
    std::vector<T> m_items;
    std::size_t m_start = 0; // the oldest item
    std::size_t m_size = 0;
    std::atomic<Revision> m_revision{ 0 };
};

} // namespace atpl::detail
