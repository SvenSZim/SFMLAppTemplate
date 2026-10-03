#include "atpl/core/series.hpp"

namespace atpl {

Series::Series(std::size_t capacity) :
    m_samples(capacity) {}

std::size_t Series::capacity() const {
    return m_samples.capacity();
}

std::size_t Series::size() const {
    return m_samples.size();
}

void Series::push(float sample) {
    m_samples.push(std::span(&sample, 1));
}

void Series::push(std::span<const float> samples) {
    m_samples.push(samples);
}

void Series::clear() {
    m_samples.clear();
}

std::size_t Series::read(std::span<float> out) const {
    return m_samples.read(out);
}

Revision Series::revision() const {
    return m_samples.revision();
}

PointSeries::PointSeries(std::size_t capacity) :
    m_points(capacity) {}

std::size_t PointSeries::capacity() const {
    return m_points.capacity();
}

std::size_t PointSeries::size() const {
    return m_points.size();
}

void PointSeries::push(Point point) {
    m_points.push(std::span(&point, 1));
}

void PointSeries::push(std::span<const Point> points) {
    m_points.push(points);
}

void PointSeries::clear() {
    m_points.clear();
}

std::size_t PointSeries::read(std::span<Point> out) const {
    return m_points.read(out);
}

Revision PointSeries::revision() const {
    return m_points.revision();
}

} // namespace atpl
