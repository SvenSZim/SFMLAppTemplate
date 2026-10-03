#include "atpl/core/text_log.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

TextLog::TextLog(std::size_t capacity) :
    m_capacity(std::max<std::size_t>(capacity, 1)) {}

std::size_t TextLog::capacity() const {
    return m_capacity;
}

std::size_t TextLog::size() const {
    const std::scoped_lock lock(m_mutex);
    return m_lines.size();
}

void TextLog::push(std::string line) {
    LogLine entry{ .text = std::move(line), .time = std::chrono::system_clock::now() };
    const std::scoped_lock lock(m_mutex);
    if (m_lines.size() == m_capacity) {
        m_lines.pop_front();
    }
    m_lines.push_back(std::move(entry));
    ++m_pushed;
    ++m_revision;
}

void TextLog::clear() {
    const std::scoped_lock lock(m_mutex);
    m_lines.clear();
    ++m_revision;
}

std::size_t TextLog::read(std::vector<LogLine>& out, std::size_t newest) const {
    const std::scoped_lock lock(m_mutex);
    const std::size_t count = std::min(newest, m_lines.size());
    out.assign(m_lines.end() - static_cast<std::ptrdiff_t>(count), m_lines.end());
    return count;
}

std::uint64_t TextLog::pushed() const {
    const std::scoped_lock lock(m_mutex);
    return m_pushed;
}

Revision TextLog::revision() const {
    const std::scoped_lock lock(m_mutex);
    return m_revision;
}

} // namespace atpl
