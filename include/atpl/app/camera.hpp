#pragma once

#include "atpl/ui/event.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <string>

namespace atpl {

/// How a `Camera` reacts to input.
struct CameraOptions {
    /// The button that moves the view while held and dragged.
    sf::Mouse::Button dragButton = sf::Mouse::Button::Left;

    /// How much one notch of the wheel zooms.
    float zoomStep = 1.1f;

    float minZoom = 0.05f;
    float maxZoom = 50.f;
};

/// Pan and zoom for one view, the way most simulations want it: drag to move, wheel to zoom
/// towards the pointer.
///
/// This is an optional helper. The UI only forwards input; a camera is one way of using it.
/// An application can have one per view, or none and handle the events itself.
///
///     atpl::Camera camera("world");
///
///     app.onEvent([&](const atpl::Event& event) {
///         if (camera.handle(event)) app.ui().requestRedraw();
///     });
///     app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
///         camera.apply(target, size);
///         drawWorld(target);        // in world coordinates
///     });
///
/// Zoom is pixels per world unit. The centre is the world position shown in the middle of the view.
class Camera {
public:
    /// A camera for the view with this name. It only reacts to input that happens in that view.
    explicit Camera(std::string viewName, CameraOptions options = {});

    /// Reacts to a forwarded event if it concerns this camera. Returns true if the camera moved
    /// or zoomed, which means the view needs to be drawn again.
    bool handle(const Event& event);

    /// Sets up `target` so that what is drawn next is in world coordinates as seen by this camera.
    /// Call it at the start of the view's draw function, with the size the function was given.
    void apply(sf::RenderTarget& target, sf::Vector2f viewSize);

    /// The world position under a point of the view, for example `pointer.inView` of an event.
    /// Uses the view size of the last `apply`.
    [[nodiscard]] sf::Vector2f toWorld(sf::Vector2f inView) const;

    [[nodiscard]] sf::Vector2f center() const;
    void setCenter(sf::Vector2f worldPosition);

    [[nodiscard]] float zoom() const;
    void setZoom(float pixelsPerUnit);

    /// Moves and zooms so that the given part of the world fills the view, with nothing cut off.
    void show(sf::Vector2f worldTopLeft, sf::Vector2f worldSize);
};

} // namespace atpl
