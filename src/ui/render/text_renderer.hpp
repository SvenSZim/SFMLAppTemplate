#pragma once

#include "ui/render/draw_list.hpp"

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <span>

namespace atpl::render {

/// Draws the text of a layer. The renderer hands each layer to this interface after drawing its
/// shapes; how text is turned into glyphs and kept between frames is decided behind it.
class TextRenderer {
public:
    virtual ~TextRenderer() = default;

    /// Draws the layer's text runs and returns how many draw calls that took.
    virtual std::size_t draw(sf::RenderTarget& target, const sf::RenderStates& states, const DrawList& layer) = 0;
};

} // namespace atpl::render
