#pragma once

#include "atpl/ui/rect.hpp"

#include "ui/render/draw_list.hpp"

#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>

namespace atpl::render {

/// What one panel draws, kept from frame to frame.
///
/// A batch holds the panel's geometry in the panel's own coordinates, with (0, 0) at its top-left
/// corner, and is rebuilt only when the panel is dirty. Everything that merely moves what is
/// already there is done when drawing, without rebuilding:
///
///   moving the panel         a different position
///   scrolling its content    a different offset of the content layer
///
/// A batch has two layers. The frame is the panel itself: background, header, scrollbar. The
/// content is its widgets; it can be scrolled, and is then clipped to the content area so that
/// nothing shows outside it.
///
/// The batch does not know what a panel or a widget is. Whoever owns it paints into the two
/// lists when `isDirty()` says so.
class PanelBatch {
public:
    // ----- Rebuilding -----

    /// A new batch is dirty: it has nothing to draw yet.
    [[nodiscard]] bool isDirty() const { return m_dirty; }

    /// Says that the panel looks different now and must be painted again.
    void markDirty() { m_dirty = true; }

    struct Layers {
        DrawList& frame;
        DrawList& content;
    };

    /// Starts painting the panel anew: empties both layers, keeping their memory, and returns
    /// them to be filled. The batch is clean afterwards.
    Layers rebuild();

    /// How often the batch has been rebuilt. For tests and for the profiler.
    [[nodiscard]] std::size_t rebuildCount() const { return m_rebuildCount; }

    // ----- Placing: none of this rebuilds, except a new size -----

    /// The panel's top-left corner in the window.
    void setPosition(sf::Vector2f position) { m_position = position; }
    [[nodiscard]] sf::Vector2f position() const { return m_position; }

    /// The panel's size. A panel of a different size looks different, so this marks the batch
    /// dirty if the size changed.
    void setSize(sf::Vector2f size);
    [[nodiscard]] sf::Vector2f size() const { return m_size; }

    /// The area the content may be seen in, in the panel's coordinates. Empty: the content is
    /// not clipped.
    void setContentClip(std::optional<FloatRect> clip) { m_contentClip = clip; }

    /// How far the content is scrolled: it is drawn this many pixels further up.
    void setScroll(float offset) { m_scroll = offset; }
    [[nodiscard]] float scroll() const { return m_scroll; }

    /// A hidden panel is not drawn. It keeps its geometry.
    void setVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool isVisible() const { return m_visible; }

    // ----- Drawing -----

    [[nodiscard]] const DrawList& frame() const { return m_frame; }
    [[nodiscard]] const DrawList& content() const { return m_content; }

    /// From the panel's coordinates to the window's, for the frame layer.
    [[nodiscard]] sf::Transform frameTransform() const;

    /// The same for the content layer: the scroll offset is included.
    [[nodiscard]] sf::Transform contentTransform() const;

    /// The content's clip area in the window's coordinates, or empty if it is not clipped.
    [[nodiscard]] std::optional<FloatRect> clipInWindow() const;

private:
    DrawList m_frame;
    DrawList m_content;
    bool m_dirty = true;
    bool m_visible = true;
    std::size_t m_rebuildCount = 0;

    sf::Vector2f m_position;
    sf::Vector2f m_size;
    std::optional<FloatRect> m_contentClip;
    float m_scroll = 0.f;
};

} // namespace atpl::render
