#include "ui/theme/color.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atpl::theme {

namespace {

[[nodiscard]] std::uint8_t toByte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.f, 255.f)));
}

[[nodiscard]] float mixChannel(std::uint8_t from, std::uint8_t to, float amount) {
    return static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * amount;
}

/// One colour channel as the amount of light it gives off (sRGB to linear).
[[nodiscard]] float linear(std::uint8_t channel) {
    const float c = static_cast<float>(channel) / 255.f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

sf::Color mix(sf::Color from, sf::Color to, float amount) {
    const float t = std::clamp(amount, 0.f, 1.f);
    return { toByte(mixChannel(from.r, to.r, t)),
             toByte(mixChannel(from.g, to.g, t)),
             toByte(mixChannel(from.b, to.b, t)),
             from.a };
}

sf::Color faded(sf::Color color, float factor) {
    color.a = toByte(static_cast<float>(color.a) * std::clamp(factor, 0.f, 1.f));
    return color;
}

float luminance(sf::Color color) {
    return 0.2126f * linear(color.r) + 0.7152f * linear(color.g) + 0.0722f * linear(color.b);
}

float contrast(sf::Color a, sf::Color b) {
    const float first = luminance(a);
    const float second = luminance(b);
    return (std::max(first, second) + 0.05f) / (std::min(first, second) + 0.05f);
}

sf::Color readableOn(sf::Color background, sf::Color first, sf::Color second) {
    return contrast(background, first) >= contrast(background, second) ? first : second;
}

} // namespace atpl::theme
