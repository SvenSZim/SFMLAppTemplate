#pragma once

#include "atpl/app/camera.hpp"
#include "atpl/ui/event.hpp"
#include "atpl/ui/rect.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>
#include <string>

namespace atpl {

/// What the pointer does on a minimap.
enum class MinimapMode {
    Pan,    ///< Pressing and dragging moves the main view: it stays centred under the pointer.
    Select, ///< Dragging marks a part of the world from corner to corner; releasing shows it.
};

/// How a `Minimap` reacts to input.
struct MinimapOptions {
    MinimapMode mode = MinimapMode::Pan;

    /// The button that pans or selects.
    sf::Mouse::Button button = sf::Mouse::Button::Left;

    /// How far an arrow key moves the main view: this share of what it shows.
    float keyStep = 0.1f;
};

/// A minimap for one view: it always shows the whole world, marks what a main camera sees, and
/// steers that camera. An optional helper, like `Camera`.
///
///     atpl::Camera camera("world");
///     atpl::Minimap minimap("minimap", camera, {{-100, -100}, {200, 200}});
///
///     app.onEvent([&](const atpl::Event& event) {
///         if (minimap.handle(event) || camera.handle(event)) app.ui().requestRedraw();
///     });
///     app.ui().view("minimap").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
///         minimap.apply(target, size);   // the whole world
///         drawWorld(target);             // in world coordinates
///         minimap.drawMarks(target, sf::Color::White);
///     });
///
/// What it does:
/// - In `Pan` mode, a press centres the main view under the pointer, and dragging keeps it there.
/// - In `Select` mode, dragging marks a part of the world from the press to the pointer; the
///   release makes the main view show it. A click without dragging centres the main view there.
/// - While the minimap's view is selected (it was clicked; see `UISetup::defaultView`), the arrow
///   keys move the main view by `keyStep` of what it shows.
/// The main view's centre never leaves the world.
class Minimap {
public:
    /// A minimap for the view with this name, steering `target`, of the world inside `world`.
    /// `target` must outlive the minimap.
    Minimap(std::string viewName, Camera& target, FloatRect world, MinimapOptions options = {});

    /// Reacts to a forwarded event if it concerns this minimap. Returns true if the main view
    /// moved or what the minimap shows changed: both views need to be drawn again.
    bool handle(const Event& event);

    /// Sets up `target` to show the whole world. Call it at the start of the minimap's draw
    /// function, with the size the function was given.
    void apply(sf::RenderTarget& target, sf::Vector2f viewSize);

    /// Draws, after the world, what the main camera sees and, while selecting, the part being
    /// marked. In world coordinates, as `apply` set them up; lines are one pixel thick.
    void drawMarks(sf::RenderTarget& target, sf::Color color) const;

    [[nodiscard]] MinimapMode mode() const;
    /// Switches the mode. A selection that is going on is dropped.
    void setMode(MinimapMode mode);

    /// The world it shows. Change it when the world grows or shrinks.
    [[nodiscard]] FloatRect world() const;
    void setWorld(FloatRect world);

    /// The world position under a point of the minimap, as of the last `apply`.
    [[nodiscard]] sf::Vector2f toWorld(sf::Vector2f inView) const;

    /// The part of the world being marked, while selecting.
    [[nodiscard]] std::optional<FloatRect> selection() const;

private:
    /// Centres the main view on `worldPosition`, kept inside the world.
    void centreOn(sf::Vector2f worldPosition);

    std::string m_viewName;
    Camera* m_target;
    FloatRect m_world;
    MinimapOptions m_options;
    Camera m_camera; ///< Its own: always the whole world.

    bool m_pressed = false;             ///< The button went down in the minimap and is still down.
    sf::Vector2f m_pointer;             ///< Where the pointer is in the minimap, followed by its moves.
    sf::Vector2f m_pressedAt;           ///< Where the press was, in the minimap.
    std::optional<sf::Vector2f> m_from; ///< While selecting: the world position of the press.
};

} // namespace atpl
