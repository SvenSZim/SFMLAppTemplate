#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/value.hpp"

#include "ui/model/store.hpp"

#include <chrono>
#include <optional>

namespace atpl::binding {

// Keeping widgets and the application's data in step, in both directions (ARCHITECTURE.md 4.5).
//
// A widget never sees what it is bound to. Each frame `sync` compares every binding's revision
// with the one its widget last got; only on a change is the new value handed to the widget
// (`Widget::setValue`) and its panel marked dirty. What the user enters comes back through
// `write`.

using Clock = std::chrono::steady_clock;

/// Throws `SetupError` if the binding does not suit the widget: a kind the widget does not work
/// with, or a read-only binding for a widget that edits its value. The message names the widget.
void requireFits(const model::Store& store, WidgetId widget, const AnyBinding& binding);

/// Binds a widget, replacing what it was bound to; with nothing, unbinds it (the widget keeps
/// what it shows). The value is handed over with the next `sync`. Checks with `requireFits`.
void attach(model::Store& store, WidgetId widget, std::optional<AnyBinding> binding);

/// Checks the bindings the widgets were given in the setup and makes them take effect.
void attachAll(model::Store& store);

/// Hands every widget whose binding changed its new value, and marks its panel dirty. A series
/// marks the panel of its graph. A widget hears of changes at most as often as its
/// `Widget::refreshInterval` says (D37). Returns whether anything was handed over.
bool sync(model::Store& store, Clock::time_point now);

/// Stores a value the user entered into the widget's binding, if it has one that can be
/// written. The widget is not handed the value back.
void write(model::WidgetSlot& slot, const Value& value);

} // namespace atpl::binding
