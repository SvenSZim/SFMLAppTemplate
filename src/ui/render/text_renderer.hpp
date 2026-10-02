#pragma once

#include "ui/render/draw_list.hpp"

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <span>

namespace atpl::render {

/// Draws text runs. The renderer hands a panel's text to this interface; how text is turned
/// into glyphs and cached is decided behind it (WP 2.5).
class TextRenderer {
public:
    virtual ~TextRenderer() = default;

    /// Draws the runs and returns how many draw calls that took.
    virtual std::size_t
    draw(sf::RenderTarget& target, const sf::RenderStates& states, std::span<const TextRun> runs) = 0;
};

} // namespace atpl::render
