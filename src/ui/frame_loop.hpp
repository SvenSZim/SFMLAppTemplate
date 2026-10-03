#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowBase.hpp>

#include <atomic>
#include <optional>

namespace atpl::frame {

// The outermost cache level: a frame is drawn only if something asked for one. An application
// whose UI is idle and whose simulation has nothing new draws no frames at all and sleeps.

/// Says whether a frame needs to be drawn.
///
/// Anything may ask for a frame, from any thread: the simulation after publishing a new state,
/// the application through `UI::requestRedraw()`. The main loop takes the request when it draws.
/// Changes inside the UI need no request: a panel that was repainted, moved or scrolled is
/// noticed when the frame is presented.
class RedrawFlag {
public:
    /// Asks for a frame. May be called from any thread, any number of times: requests do not
    /// add up, the next frame serves them all.
    void request() { m_requested.store(true, std::memory_order_release); }

    /// Whether a frame was asked for since the last `take`, and clears the request.
    /// For the thread that draws.
    [[nodiscard]] bool take() { return m_requested.exchange(false, std::memory_order_acq_rel); }

    /// Whether a frame is asked for, without clearing the request.
    [[nodiscard]] bool isSet() const { return m_requested.load(std::memory_order_acquire); }

private:
    std::atomic<bool> m_requested{ true }; // the first frame always has to be drawn
};

/// How long the main loop sleeps when there is nothing to do: about one frame of a 60 Hz display.
/// A request for a frame from another thread is picked up after this time at the latest.
inline const sf::Time defaultIdleWait = sf::milliseconds(16);

/// The window's next event.
///
/// While a frame is asked for, this does not wait: it returns the next pending event, or nothing
/// if there is none. While no frame is asked for, it sleeps until an event arrives, for
/// `idleWait` at most. This is what keeps an idle application from using the processor.
///
/// Events after which the window's content has to be drawn again (a resize, the window coming
/// back into focus) ask for a frame themselves.
///
/// Only the first event of a pass may be waited for. Take the rest with `pendingEvent`, which
/// never waits: a loop that waits for every event would never end while the pointer moves, and
/// nothing would be drawn in the meantime.
///
///     for (auto event = frame::nextEvent(window, flag); event; event = frame::pendingEvent(window, flag)) {
///         ...
///     }
[[nodiscard]] std::optional<sf::Event>
nextEvent(sf::WindowBase& window, RedrawFlag& flag, sf::Time idleWait = defaultIdleWait);

/// The window's next event if there is one, without waiting. Events after which the window has
/// to be drawn again ask for a frame, as with `nextEvent`.
[[nodiscard]] std::optional<sf::Event> pendingEvent(sf::WindowBase& window, RedrawFlag& flag);

} // namespace atpl::frame
