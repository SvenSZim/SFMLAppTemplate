#include "ui/binding/sync.hpp"

#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"

#include <string>
#include <utility>

namespace atpl::binding {

namespace {

[[nodiscard]] std::string nameOf(ValueKind kind) {
    switch (kind) {
        case ValueKind::Bool:
            return "an on/off value";
        case ValueKind::Number:
            return "a number";
        case ValueKind::Index:
            return "a choice";
        case ValueKind::Text:
            return "text";
        case ValueKind::Series:
            break;
    }
    return "a series";
}

} // namespace

void requireFits(const model::Store& store, WidgetId widget, const AnyBinding& binding) {
    const model::WidgetSlot& slot = store.widget(widget);
    const std::string who = "widget \"" + store.panel(slot.panel).name + "/" + slot.name + "\"";
    if (!slot.widget->accepts(binding.kind())) {
        throw SetupError(who + " cannot be bound to " + nameOf(binding.kind()));
    }
    if (slot.widget->editsValue() && binding.isReadOnly()) {
        throw SetupError(who + " changes its value, but the binding can only be read");
    }
}

void attach(model::Store& store, WidgetId widget, std::optional<AnyBinding> binding) {
    if (binding.has_value()) {
        requireFits(store, widget, *binding);
    }
    model::WidgetSlot& slot = store.widget(widget);
    slot.binding = std::move(binding);
    slot.synced = false; // hand over the value with the next sync, whatever its revision
    slot.nextHandOver = {};
    slot.widget->setSeries(slot.binding.has_value() ? slot.binding->series() : nullptr);
    store.panel(slot.panel).dirty = true;
}

void attachAll(model::Store& store) {
    for (std::uint32_t i = 0; i < store.widgets().size(); ++i) {
        model::WidgetSlot& slot = store.widget(WidgetId{ i });
        if (slot.binding.has_value()) {
            attach(store, WidgetId{ i }, slot.binding);
        }
    }
}

bool sync(model::Store& store, Clock::time_point now) {
    bool handed = false;
    for (model::WidgetSlot& slot : store.widgets()) {
        if (!slot.binding.has_value()) {
            continue;
        }
        const Revision revision = slot.binding->revision();
        if (slot.synced && revision == slot.revisionSeen) {
            continue; // nothing new: no work at all
        }

        // Not more often than the widget asks for. The change is not lost: it is still new on
        // the next pass.
        if (slot.synced && now < slot.nextHandOver) {
            continue;
        }

        // A series: the graph reads it itself; it only needs painting again.
        if (slot.binding->kind() != ValueKind::Series) {
            if (const std::optional<Value> value = slot.binding->get()) {
                slot.widget->setValue(*value);
            }
        }
        slot.revisionSeen = revision;
        slot.synced = true;
        slot.nextHandOver = now + slot.widget->refreshInterval();
        store.panel(slot.panel).dirty = true;
        handed = true;
    }
    return handed;
}

void write(model::WidgetSlot& slot, const Value& value) {
    if (!slot.binding.has_value() || slot.binding->isReadOnly()) {
        return;
    }
    slot.binding->set(value);
    slot.revisionSeen = slot.binding->revision(); // the widget has this value already
}

} // namespace atpl::binding
