#include "atpl/core/thread_pool.hpp"

#include <algorithm>
#include <utility>

namespace atpl {

// How a loop runs: the caller posts the job under the mutex and wakes the workers. Every thread
// that joins it (the caller included) takes parts one after another from m_nextPart until none
// are left. Then the caller withdraws the job, so no late worker joins it any more, and waits
// until every worker that did join has left. Only then is m_nextPart reset for the next job, so
// a worker can never take a part of one job while running the code of another.

namespace {

/// Set while this thread runs a part, so that a loop inside a part runs on this thread alone.
thread_local bool t_inPart = false;

/// Marks this thread as running parts for as long as it lives, then restores what was before.
class InPart {
public:
    InPart() :
        m_outer(std::exchange(t_inPart, true)) {}
    ~InPart() { t_inPart = m_outer; }
    InPart(const InPart&) = delete;
    InPart& operator=(const InPart&) = delete;

private:
    bool m_outer;
};

} // namespace

ThreadPool::ThreadPool() :
    ThreadPool(defaultWorkerCount()) {}

ThreadPool::ThreadPool(std::size_t workers) {
    m_workers.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        const std::lock_guard call(m_callMutex);
        const std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_wake.notify_all();
    for (std::thread& worker : m_workers) {
        worker.join();
    }
}

std::size_t ThreadPool::defaultWorkerCount() {
    const std::size_t hardware = std::thread::hardware_concurrency();
    return hardware > 2 ? hardware - 2 : 0;
}

std::size_t ThreadPool::workerCount() const {
    return m_workers.size();
}

std::size_t ThreadPool::maxParts() const {
    return m_workers.size() + 1;
}

std::size_t ThreadPool::partCount(std::size_t count, std::size_t minPartSize) const {
    if (count == 0) {
        return 0;
    }
    const std::size_t bySize = count / std::max<std::size_t>(minPartSize, 1);
    return std::clamp<std::size_t>(bySize, 1, maxParts());
}

void ThreadPool::run(const Job& job) {
    if (job.parts == 1 || m_workers.empty() || t_inPart) {
        std::exception_ptr failure;
        {
            const InPart inPart;
            for (std::size_t part = 0; part < job.parts; ++part) {
                try {
                    job.call(job.context, part);
                } catch (...) {
                    if (!failure) {
                        failure = std::current_exception();
                    }
                }
            }
        }
        if (failure) {
            std::rethrow_exception(failure);
        }
        return;
    }

    const std::lock_guard call(m_callMutex);
    {
        const std::lock_guard lock(m_mutex);
        m_job = &job;
        ++m_generation;
        m_nextPart = 0;
        m_failure = nullptr;
    }
    m_wake.notify_all();

    workOn(job);

    std::exception_ptr failure;
    {
        std::unique_lock lock(m_mutex);
        m_job = nullptr;
        m_left.wait(lock, [this] { return m_joined == 0; });
        failure = std::exchange(m_failure, nullptr);
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void ThreadPool::workOn(const Job& job) {
    const InPart inPart;
    for (;;) {
        std::size_t part = 0;
        {
            const std::lock_guard lock(m_mutex);
            if (m_nextPart >= job.parts) {
                break;
            }
            part = m_nextPart++;
        }
        try {
            job.call(job.context, part);
        } catch (...) {
            const std::lock_guard lock(m_mutex);
            if (!m_failure) {
                m_failure = std::current_exception();
            }
        }
    }
}

void ThreadPool::workerLoop() {
    std::size_t seen = 0;
    for (;;) {
        const Job* job = nullptr;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [&] { return m_stopping || (m_job != nullptr && m_generation != seen); });
            if (m_stopping) {
                return;
            }
            seen = m_generation;
            job = m_job;
            ++m_joined;
        }
        workOn(*job);
        {
            const std::lock_guard lock(m_mutex);
            --m_joined;
        }
        m_left.notify_one();
    }
}

} // namespace atpl
