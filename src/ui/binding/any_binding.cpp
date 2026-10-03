#include "atpl/ui/binding.hpp"
#include "atpl/ui/error.hpp"
#include "atpl/ui/value.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// A binding made of functions. The getter is polled: the revision grows when what it returns
/// differs from what it returned before.
template <typename T>
class FunctionBinding final : public Binding<T> {
public:
    FunctionBinding(std::function<T()> get, std::function<void(T)> set) :
        m_get(std::move(get)),
        m_set(std::move(set)) {}

    [[nodiscard]] T get() const override { return m_get(); }

    void set(const T& value) override {
        if (m_set) {
            m_set(value);
        }
    }

    [[nodiscard]] Revision revision() const override {
        T now = m_get();
        if (!m_seen || !(now == m_last)) {
            m_last = std::move(now);
            m_seen = true;
            ++m_revision;
        }
        return m_revision;
    }

    [[nodiscard]] bool isReadOnly() const override { return !m_set; }

private:
    std::function<T()> m_get;
    std::function<void(T)> m_set;
    mutable T m_last{};
    mutable bool m_seen = false;
    mutable Revision m_revision = 0;
};

/// A `Series` or a `PointSeries` as the binding of a graph.
class SamplesBinding final : public SeriesBinding {
public:
    explicit SamplesBinding(Series& series) :
        m_series(&series) {}

    std::size_t read(std::span<float> out) const override { return m_series->read(out); }
    [[nodiscard]] Revision revision() const override { return m_series->revision(); }

private:
    Series* m_series;
};

class PointsBinding final : public SeriesBinding {
public:
    explicit PointsBinding(PointSeries& points) :
        m_points(&points),
        m_scratch(points.capacity()) {}

    // Asked for samples, a series of points gives its y values. They are read through a buffer
    // the size of the series, made once: nothing is allocated per read. (Used on the main thread
    // only, like every binding.)
    std::size_t read(std::span<float> out) const override {
        const std::size_t count = m_points->read(std::span(m_scratch).first(std::min(out.size(), m_scratch.size())));
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = m_scratch[i].y;
        }
        return count;
    }

    [[nodiscard]] Revision revision() const override { return m_points->revision(); }
    [[nodiscard]] bool hasPoints() const override { return true; }
    std::size_t readPoints(std::span<Point> out) const override { return m_points->read(out); }

private:
    PointSeries* m_points;
    mutable std::vector<Point> m_scratch;
};

/// A `TextLog` as the binding of a log.
class LogBinding final : public LinesBinding {
public:
    explicit LogBinding(TextLog& log) :
        m_log(&log) {}

    std::size_t read(std::vector<LogLine>& out, std::size_t newest) const override { return m_log->read(out, newest); }
    [[nodiscard]] std::size_t size() const override { return m_log->size(); }
    [[nodiscard]] std::uint64_t pushed() const override { return m_log->pushed(); }
    [[nodiscard]] Revision revision() const override { return m_log->revision(); }

private:
    TextLog* m_log;
};

[[nodiscard]] std::string nameOf(ValueKind kind) {
    switch (kind) {
        case ValueKind::Bool:
            return "on/off";
        case ValueKind::Number:
            return "number";
        case ValueKind::Index:
            return "choice";
        case ValueKind::Text:
            return "text";
        case ValueKind::Series:
            return "series";
        case ValueKind::Lines:
            break;
    }
    return "lines";
}

} // namespace

AnyBinding::AnyBinding(Target target, std::shared_ptr<void> owned) :
    m_target(target),
    m_owned(std::move(owned)) {}

AnyBinding::AnyBinding(Series& series) :
    AnyBinding(owning(std::make_shared<SamplesBinding>(series))) {}

AnyBinding::AnyBinding(PointSeries& series) :
    AnyBinding(owning(std::make_shared<PointsBinding>(series))) {}

AnyBinding::AnyBinding(TextLog& log) :
    AnyBinding(owning(std::make_shared<LogBinding>(log))) {}

AnyBinding::AnyBinding(LinesBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding::AnyBinding(BoolBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding::AnyBinding(NumberBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding::AnyBinding(IndexBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding::AnyBinding(TextBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding::AnyBinding(SeriesBinding& binding) :
    AnyBinding(Target(&binding), nullptr) {}

AnyBinding AnyBinding::ofBool(std::function<bool()> get, std::function<void(bool)> set) {
    return owning(std::make_shared<FunctionBinding<bool>>(std::move(get), std::move(set)));
}

AnyBinding AnyBinding::ofNumber(std::function<double()> get, std::function<void(double)> set) {
    return owning(std::make_shared<FunctionBinding<double>>(std::move(get), std::move(set)));
}

AnyBinding AnyBinding::ofIndex(std::function<std::size_t()> get, std::function<void(std::size_t)> set) {
    return owning(std::make_shared<FunctionBinding<std::size_t>>(std::move(get), std::move(set)));
}

AnyBinding AnyBinding::ofText(std::function<std::string()> get, std::function<void(const std::string&)> set) {
    std::function<void(std::string)> store;
    if (set) {
        store = [set = std::move(set)](const std::string& value) { set(value); };
    }
    return owning(std::make_shared<FunctionBinding<std::string>>(std::move(get), std::move(store)));
}

ValueKind AnyBinding::kind() const {
    switch (m_target.index()) {
        case 0:
            return ValueKind::Bool;
        case 1:
            return ValueKind::Number;
        case 2:
            return ValueKind::Index;
        case 3:
            return ValueKind::Text;
        case 4:
            return ValueKind::Series;
        default:
            break;
    }
    return ValueKind::Lines;
}

bool AnyBinding::isReadOnly() const {
    return std::visit(
        [](const auto* binding) {
            if constexpr (requires { binding->isReadOnly(); }) {
                return binding->isReadOnly();
            } else {
                return true; // a series or a log is only shown
            }
        },
        m_target
    );
}

std::optional<Value> AnyBinding::get() const {
    return std::visit(
        [](const auto* binding) -> std::optional<Value> {
            if constexpr (requires { binding->get(); }) {
                return Value(binding->get());
            } else {
                return std::nullopt;
            }
        },
        m_target
    );
}

void AnyBinding::set(const Value& value) const {
    std::visit(
        [&value](auto* binding) {
            if constexpr (requires { binding->get(); }) {
                using Kind = std::remove_cvref_t<decltype(binding->get())>;
                if (!binding->isReadOnly()) {
                    if (const auto* typed = std::get_if<Kind>(&value)) {
                        binding->set(*typed);
                    }
                }
            }
        },
        m_target
    );
}

Revision AnyBinding::revision() const {
    return std::visit([](const auto* binding) { return binding->revision(); }, m_target);
}

const SeriesBinding* AnyBinding::series() const {
    const auto* const* series = std::get_if<SeriesBinding*>(&m_target);
    return series != nullptr ? *series : nullptr;
}

const LinesBinding* AnyBinding::lines() const {
    const auto* const* lines = std::get_if<LinesBinding*>(&m_target);
    return lines != nullptr ? *lines : nullptr;
}

ValueKind kindOf(const Value& value) {
    switch (value.index()) {
        case 0:
            return ValueKind::Bool;
        case 1:
            return ValueKind::Number;
        case 2:
            return ValueKind::Index;
        default:
            break;
    }
    return ValueKind::Text;
}

namespace detail {

void wrongKind(const Value& value, ValueKind wanted) {
    throw SetupError("a value of kind " + nameOf(kindOf(value)) + " cannot be read as one of kind " + nameOf(wanted));
}

} // namespace detail

} // namespace atpl
