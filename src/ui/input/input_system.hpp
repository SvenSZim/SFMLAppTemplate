#pragma once

#include "atpl/ui/event.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/layout.hpp"

#include "ui/model/store.hpp"

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <optional>
#include <span>
#include <vector>

namespace atpl::input {

/// The one owner of interaction state: what the pointer is over, what is pressed, what has the
/// pointer captured, what has the keyboard focus, and what the UI keeps for itself.
///
/// Rules (ARCHITECTURE.md 4.8):
/// - Pointer input over a panel is the UI's: it goes to the widget under the pointer, and is not
///   forwarded. Pointer input anywhere else is forwarded to the application.
/// - A press and everything up to its release go to the same place. A drag that starts on a
///   widget goes to that widget wherever the pointer goes, and is never forwarded; a drag that
///   starts outside the panels is forwarded even where it crosses one.
/// - The wheel over a panel is the UI's, whether anything in the panel uses it or not.
/// - Keys and text go to the widget with the keyboard focus, if there is one, and are forwarded
///   otherwise. A press anywhere but on the focused widget takes the focus away.
/// - Window events are not handled here; the UI forwards them itself.
///
/// What changes how a widget looks (hovered, pressed, focused) marks its panel dirty.
class InputSystem {
public:
    /// Handles one event of the window. Widget events and forwarded input are appended to
    /// `events`. `stacking` is the panels from the bottom to the top (`Store::stackingOrder`).
    void handle(
        const sf::Event& event,
        model::Store& store,
        std::span<const PanelId> stacking,
        const Sizes& sizes,
        std::vector<Event>& events
    );

    /// Forgets interaction state that may no longer be true: after the panels were laid out
    /// anew, a widget's place may have changed under a pointer that did not move.
    void forgetHover(model::Store& store);

    [[nodiscard]] std::optional<WidgetId> hovered() const { return m_hovered; }
    [[nodiscard]] std::optional<WidgetId> pressed() const { return m_pressed; }
    [[nodiscard]] std::optional<WidgetId> focused() const { return m_focused; }
    [[nodiscard]] std::optional<WidgetId> captured() const { return m_captured; }

    // ----- What a widget does through its context -----

    void markDirty(WidgetId widget);
    void capturePointer(WidgetId widget);
    void releasePointer(WidgetId widget);
    void requestFocus(WidgetId widget);
    void releaseFocus(WidgetId widget);
    void changeValue(WidgetId widget, Value value, bool final);
    void press(WidgetId widget);

private:
    /// Who a press, and everything up to its release, belongs to.
    enum class Owner { None, Ui, Application };

    struct Hit {
        std::optional<PanelId> panel;
        std::optional<WidgetId> widget; ///< Only widgets that are drawn and enabled.
    };

    [[nodiscard]] Hit hitTest(sf::Vector2f position) const;
    [[nodiscard]] PointerLocation locate(sf::Vector2f position) const;
    [[nodiscard]] sf::Vector2f contentOrigin(PanelId panel) const;

    void setHover(const Hit& hit);
    void setFocus(std::optional<WidgetId> widget);
    void setPressed(std::optional<WidgetId> widget);

    /// Hands an event to a widget, with a context for it.
    void deliver(WidgetId widget, const Event& event);

    void forward(Event event);

    // What the event being handled works with.
    model::Store* m_store = nullptr;
    std::span<const PanelId> m_stacking;
    const Sizes* m_sizes = nullptr;
    std::vector<Event>* m_events = nullptr;

    // The interaction state this class owns.
    sf::Vector2f m_pointer;
    std::optional<sf::Vector2f> m_lastForwarded; ///< Where the last forwarded move ended.
    std::optional<PanelId> m_hoveredPanel;
    std::optional<WidgetId> m_hovered;
    std::optional<WidgetId> m_pressed;
    std::optional<WidgetId> m_captured;
    std::optional<WidgetId> m_focused;
    Owner m_owner = Owner::None;
    int m_buttonsDown = 0;
};

} // namespace atpl::input
