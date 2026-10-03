#pragma once

#include "atpl/core/revision.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace atpl {

/// One line of a `TextLog`, with the moment it was pushed.
struct LogLine {
    std::string text;
    std::chrono::system_clock::time_point time;

    [[nodiscard]] friend bool operator==(const LogLine&, const LogLine&) = default;
};

/// A running log of text lines shared between threads: the data behind a `Log` widget.
///
/// Holds the newest `capacity` lines; pushing more drops the oldest. The application or the
/// simulation pushes what happens, and a log widget shows the newest lines.
///
///     TextLog events(200);
///     events.push("Reset");                          // any thread
///
///     std::vector<LogLine> newest;                   // main thread
///     events.read(newest, 6);
///
/// Thread safety: any thread may push, read or clear at any time.
class TextLog {
public:
    /// A log that keeps the newest `capacity` lines (at least one).
    explicit TextLog(std::size_t capacity);

    TextLog(const TextLog&) = delete;
    TextLog(TextLog&&) = delete;
    TextLog& operator=(const TextLog&) = delete;
    TextLog& operator=(TextLog&&) = delete;

    /// The largest number of lines kept.
    [[nodiscard]] std::size_t capacity() const;

    /// The number of lines currently held, at most `capacity()`.
    [[nodiscard]] std::size_t size() const;

    /// Appends a line, stamped with the current time. If the log is full, the oldest line is
    /// dropped. A line break in the text does not start a new line of the log.
    void push(std::string line);

    /// Removes all lines.
    void clear();

    /// Replaces the contents of `out` with the newest `newest` lines (or all there are), oldest
    /// first, and returns how many.
    std::size_t read(std::vector<LogLine>& out, std::size_t newest) const;

    /// How many lines were ever pushed, including those dropped since. Never goes down, also not
    /// with `clear`: a reader that remembers it knows how many lines came since.
    [[nodiscard]] std::uint64_t pushed() const;

    /// Grows with every push and every clear.
    [[nodiscard]] Revision revision() const;

private:
    mutable std::mutex m_mutex;
    std::deque<LogLine> m_lines;
    std::size_t m_capacity;
    std::uint64_t m_pushed = 0;
    Revision m_revision = 0;
};

} // namespace atpl
