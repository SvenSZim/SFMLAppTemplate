#pragma once

#include <SFML/Graphics/Color.hpp>

namespace atpl::theme {

// Colour arithmetic for deriving one colour from another.

/// The colour `amount` of the way from `from` to `to`: 0 gives `from`, 1 gives `to`.
/// The transparency of `from` is kept.
[[nodiscard]] sf::Color mix(sf::Color from, sf::Color to, float amount);

/// The same colour, `factor` times as opaque.
[[nodiscard]] sf::Color faded(sf::Color color, float factor);

/// How bright a colour looks, from 0 (black) to 1 (white). The relative luminance of WCAG 2.
[[nodiscard]] float luminance(sf::Color color);

/// How well two colours can be told apart, from 1 (the same) to 21 (black on white).
/// The contrast ratio of WCAG 2: text needs about 4.5, large shapes about 3.
[[nodiscard]] float contrast(sf::Color a, sf::Color b);

/// Whichever of the two candidates stands out more against `background`.
[[nodiscard]] sf::Color readableOn(sf::Color background, sf::Color first, sf::Color second);

} // namespace atpl::theme
