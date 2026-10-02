#pragma once

#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"

#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include <span>
#include <vector>

namespace atpl::render {

// Turns shapes into triangles.
//
// Everything the UI draws ends up as triangles in one list per panel, so a panel is drawn with a
// single call. These functions only append to a list the caller owns and reuses; they keep no
// state and allocate nothing themselves.

/// Triangles as a flat list: every three vertices are one triangle.
using VertexList = std::vector<sf::Vertex>;

/// How many straight pieces a quarter circle of this radius is drawn with. Chosen so the pieces
/// stay within a quarter of a pixel of the true circle: small radii need few, large ones more.
/// 0 for a radius of 0, where a corner is a single point.
[[nodiscard]] int cornerSegments(float radius);

/// A box: shadow, fill and outline, as the style says.
///
/// - The radius is limited to half the box's smaller side, so `fullyRound` gives a pill or a circle.
/// - The outline is drawn inside the box: the box never gets larger than `rect`.
/// - The shadow is drawn first, so the box covers the part of it that lies underneath.
/// - Nothing is added for a style that is not shown, or for a box without area.
void appendBox(VertexList& out, const FloatRect& rect, const PartStyle& style);

/// A straight line of the style's thickness and colour, with flat ends.
void appendLine(VertexList& out, sf::Vector2f from, sf::Vector2f to, const PartStyle& style);

/// Connected lines through all points, joined without gaps at the bends.
/// Always adds `(points.size() - 1) * 6` vertices, or none for fewer than two points.
void appendPolyline(VertexList& out, std::span<const sf::Vector2f> points, const PartStyle& style);

} // namespace atpl::render
