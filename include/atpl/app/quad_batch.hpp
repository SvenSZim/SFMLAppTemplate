#pragma once

#include "atpl/ui/rect.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace atpl {

/// Many rectangles, plain or textured, drawn in one call: for grid cells, particles, sprites.
///
///     QuadBatch cells;                               // built once: a cell per grid cell
///     for (int y = 0; y < grid.height(); ++y) {
///         for (int x = 0; x < grid.width(); ++x) {
///             cells.add(FloatRect(x * size, y * size, size, size), sf::Color::Black);
///         }
///     }
///     cells.setColor(grid.index(x, y), heatColour); // every frame, only what changes
///     target.draw(cells);                            // all of them, one draw call
///
///     QuadBatch ants(&app.resources().texture("textures/ant.png"));
///     ants.clear();                                  // every frame, rebuilt
///     for (const Ant& ant : world.ants) {
///         ants.add(ant.position, { 8.f, 8.f }, ant.heading, sf::Color::White);
///     }
///
/// A quad is the rectangle's four corners, top-left, top-right, bottom-right, bottom-left, each
/// with a colour and a point of the texture (in its pixels). SFML has no quads, so each is two
/// triangles: six vertices. All quads of a batch share one texture (or none), and the batch is
/// drawn with the target's current view, so coordinates are world units under a camera.
///
/// Thread safety: like a vector. After `resize(n)`, different threads may `set` different quads
/// at once, so a `parallelFor` can fill a batch; drawing belongs to the main thread.
class QuadBatch : public sf::Drawable {
public:
    /// A batch without quads, drawn with `texture` (or plain colours without one). The texture
    /// must outlive the batch, or be replaced with `setTexture` before it is gone.
    explicit QuadBatch(const sf::Texture* texture = nullptr);

    [[nodiscard]] const sf::Texture* texture() const { return m_texture; }

    /// Draws with another texture from now on. The texture points of the quads stay as they were.
    void setTexture(const sf::Texture* texture);

    /// The number of quads.
    [[nodiscard]] std::size_t size() const { return m_vertices.size() / verticesPerQuad; }
    [[nodiscard]] bool empty() const { return m_vertices.empty(); }

    /// Removes every quad; the memory is kept for the next round.
    void clear();

    /// Makes room for `quads` quads without adding any.
    void reserve(std::size_t quads);

    /// Makes it `quads` quads: new ones are empty (nothing shows) until they are `set`.
    void resize(std::size_t quads);

    /// Adds a rectangle in one colour, and returns its index. In a textured batch it shows the
    /// whole texture, tinted by `color`.
    std::size_t add(FloatRect rect, sf::Color color = sf::Color::White);

    /// Adds a rectangle that shows `textureRect` (in the texture's pixels), tinted by `color`.
    std::size_t add(FloatRect rect, FloatRect textureRect, sf::Color color = sf::Color::White);

    /// Adds a rectangle of `size` around `centre`, turned by `angle` (clockwise on screen, as
    /// SFML turns), showing the whole texture or `textureRect` of it.
    std::size_t add(sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, sf::Color color = sf::Color::White);
    std::size_t
    add(sf::Vector2f centre,
        sf::Vector2f size,
        sf::Angle angle,
        FloatRect textureRect,
        sf::Color color = sf::Color::White);

    /// Replaces quad `index` with what the `add` of the same arguments would add.
    void set(std::size_t index, FloatRect rect, sf::Color color = sf::Color::White);
    void set(std::size_t index, FloatRect rect, FloatRect textureRect, sf::Color color = sf::Color::White);
    void
    set(std::size_t index, sf::Vector2f centre, sf::Vector2f size, sf::Angle angle, sf::Color color = sf::Color::White);
    void
    set(std::size_t index,
        sf::Vector2f centre,
        sf::Vector2f size,
        sf::Angle angle,
        FloatRect textureRect,
        sf::Color color = sf::Color::White);

    /// Sets the corners of quad `index` freely: top-left, top-right, bottom-right, bottom-left.
    void
    set(std::size_t index,
        const std::array<sf::Vector2f, 4>& corners,
        const std::array<sf::Color, 4>& colors,
        const std::array<sf::Vector2f, 4>& texturePoints = {});

    /// Gives quad `index` one colour; nothing else changes.
    void setColor(std::size_t index, sf::Color color);

    /// Gives the corners of quad `index` their own colours, for a gradient: top-left, top-right,
    /// bottom-right, bottom-left.
    void setColors(std::size_t index, const std::array<sf::Color, 4>& colors);

    /// The vertices, six per quad: what is drawn.
    [[nodiscard]] std::span<const sf::Vertex> vertices() const { return m_vertices; }

    static constexpr std::size_t verticesPerQuad = 6;

protected:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    /// The rectangle of the whole texture, or nothing without one.
    [[nodiscard]] FloatRect wholeTexture() const;

    std::vector<sf::Vertex> m_vertices;
    const sf::Texture* m_texture = nullptr;
};

} // namespace atpl
