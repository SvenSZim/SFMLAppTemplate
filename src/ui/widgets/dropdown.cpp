#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// A choice from a list: its label above, the field with the chosen entry below. Its list opens
/// in the overlay, above every panel, and shows as many entries as there is room for, up to
/// `maxVisible`; the rest scroll.
class DropdownWidget final : public Widget {
public:
    DropdownWidget(std::string label, std::vector<std::string> entries, DropdownOptions options) :
        m_label(std::move(label)),
        m_entries(std::move(entries)),
        m_options(std::move(options)),
        m_selected(m_options.initial) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        sf::Vector2f widest;
        for (const std::string& entry : m_entries) {
            const sf::Vector2f text = context.textSize(entry, Dropdown::Selected);
            widest = { std::max(widest.x, text.x), std::max(widest.y, text.y) };
        }
        const sf::Vector2f label = context.textSize(m_label, Dropdown::Label);
        return widgets::labelledField(context.sizes(), label, { widest.x + context.sizes().rowHeight, widest.y });
    }

    bool handleInput(const Event& event, InputContext& context) override {
        const bool open = has(context.state(), State::Open);
        if (open) {
            clampScroll(visibleIn(context), false); // the list may have been placed with less room
        }
        const FloatRect field = widgets::fieldBelow(context.size(), context.textSize(m_label, Dropdown::Label).y);

        if (const auto* press = event.getIf<PointerPressed>()) {
            if (press->button != sf::Mouse::Button::Left) {
                return true;
            }
            const sf::Vector2f at = context.local(press->pointer);
            if (!open) {
                if (field.contains(at)) {
                    m_highlight = m_selected;
                    m_first = m_selected; // shown from the chosen entry on, as far as the list allows
                    context.openOverlay();
                    context.requestFocus(); // for the keys
                }
                return true;
            }
            if (field.contains(at)) {
                close(context); // a second click on the field
            } else {
                highlightAt(at, context);
            }
            return true;
        }
        if (const auto* move = event.getIf<PointerMoved>()) {
            if (open) {
                highlightAt(context.local(move->pointer), context);
            }
            return true;
        }
        if (const auto* release = event.getIf<PointerReleased>()) {
            if (open && release->button == sf::Mouse::Button::Left) {
                if (const std::optional<std::size_t> entry = entryAt(context.local(release->pointer), context)) {
                    choose(*entry, context); // a click on an entry, or a press dragged to it
                }
            }
            return true;
        }
        if (const auto* wheel = event.getIf<Scrolled>()) {
            if (open) {
                const int steps = static_cast<int>(std::round(wheel->delta));
                const auto first = static_cast<long long>(m_first) - steps;
                m_first = static_cast<std::size_t>(std::max(first, 0LL));
                clampScroll(visibleIn(context), false);
                context.markDirty();
            }
            return true;
        }
        if (const auto* key = event.getIf<KeyPressed>()) {
            if (open) {
                handleKey(key->key, context);
            }
            return true;
        }
        return false;
    }

    void focusLost(InputContext& context) override { context.closeOverlay(); }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const PartStyle labelStyle = style.part(Dropdown::Label);
        const float labelHeight = painter.textSize(m_label, labelStyle).y;
        painter.text(FloatRect(0.f, 0.f, size.x, labelHeight), m_label, labelStyle);

        const FloatRect field = widgets::fieldBelow(size, labelHeight);
        painter.box(field, style.part(Dropdown::Field));

        // The chosen entry at the left, the arrow in a square at the right.
        const float arrowRoom = field.height();
        const float inset = std::max(style.sizes().padding.x * 0.6f, 2.f);
        painter.text(
            FloatRect(
                field.left() + inset, field.top(), std::max(field.width() - inset - arrowRoom, 0.f), field.height()
            ),
            m_entries[m_selected],
            style.part(Dropdown::Selected)
        );

        const PartStyle arrow = style.part(Dropdown::Arrow);
        if (arrow.shown) {
            // A chevron pointing down, or up while the list is open.
            const sf::Vector2f centre(field.right() - arrowRoom * 0.5f, field.top() + field.height() * 0.5f);
            const float arm = std::max(field.height() * 0.14f, 2.f);
            const float direction = has(style.state(), State::Open) ? -1.f : 1.f;
            const sf::Vector2f tip(centre.x, centre.y + arm * 0.5f * direction);
            painter.line({ tip.x - arm, tip.y - arm * direction }, tip, arrow);
            painter.line(tip, { tip.x + arm, tip.y - arm * direction }, arrow);
        }
    }

    [[nodiscard]] sf::Vector2f overlaySize(const MeasureContext& context, float maxHeight) const override {
        const Sizes& sizes = context.sizes();
        const float entry = entryHeight(sizes);
        const float pad = listPadding(sizes);
        const auto fit = static_cast<std::size_t>(std::max(std::floor((maxHeight - pad * 2.f) / entry), 1.f));
        const std::size_t shown = std::min({ m_entries.size(), m_options.maxVisible, fit });
        return { 0.f, static_cast<float>(shown) * entry + pad * 2.f };
    }

    void paintOverlay(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const Sizes& sizes = style.sizes();
        // The list covers what is below it: also in the gap a theme may leave between its
        // outline and its fill, which would let the panels below show through.
        const PartStyle list = style.part(Dropdown::List);
        PartStyle backing = list;
        backing.borderThickness = 0.f;
        backing.shadow = {};
        painter.box(FloatRect({ 0.f, 0.f }, size), backing);
        painter.box(FloatRect({ 0.f, 0.f }, size), list);

        const std::size_t shown = visibleIn(size.y, sizes);
        clampScroll(shown, false);
        const float entry = entryHeight(sizes);
        const float pad = listPadding(sizes);
        const float scrollbarRoom = shown < m_entries.size() ? std::max(sizes.padding.x * 0.5f, 3.f) : 0.f;
        const float inset = std::max(sizes.padding.x * 0.6f, 2.f);
        for (std::size_t i = m_first; i < m_first + shown; ++i) {
            const FloatRect row(
                pad, pad + static_cast<float>(i - m_first) * entry, size.x - pad * 2.f - scrollbarRoom, entry
            );
            const bool highlighted = i == m_highlight;
            if (highlighted) {
                PartStyle highlight = style.part(Dropdown::Highlight, State::Active);
                highlight.radius = std::min(highlight.radius, entry * 0.5f);
                painter.box(row, highlight);
            }
            painter.text(
                FloatRect(row.left() + inset, row.top(), std::max(row.width() - inset * 2.f, 0.f), row.height()),
                m_entries[i],
                style.part(Dropdown::Entry, highlighted ? State::Active : State::Normal)
            );
        }

        // A long list shows how far it is scrolled.
        if (scrollbarRoom > 0.f) {
            const PartStyle bar = style.part(Dropdown::Scrollbar);
            const float track = size.y - pad * 2.f;
            const float length = track * static_cast<float>(shown) / static_cast<float>(m_entries.size());
            const float top = pad + track * static_cast<float>(m_first) / static_cast<float>(m_entries.size());
            const float x = size.x - pad - scrollbarRoom * 0.5f;
            painter.line({ x, top }, { x, top + length }, bar);
        }
    }

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Index; }
    [[nodiscard]] bool editsValue() const override { return true; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_selected); }

    void setValue(const Value& value) override {
        if (const auto* index = std::get_if<std::size_t>(&value)) {
            m_selected = std::min(*index, m_entries.size() - 1);
        }
    }

private:
    [[nodiscard]] static float entryHeight(const Sizes& sizes) { return std::max(sizes.rowHeight * 0.9f, 8.f); }
    [[nodiscard]] static float listPadding(const Sizes& sizes) {
        return std::max(std::round(sizes.rowHeight * 0.12f), 2.f);
    }

    /// How many entries a list this high shows.
    [[nodiscard]] std::size_t visibleIn(float height, const Sizes& sizes) const {
        const float rows = (height - listPadding(sizes) * 2.f) / entryHeight(sizes);
        return std::clamp<std::size_t>(
            static_cast<std::size_t>(std::max(std::floor(rows + 0.01f), 1.f)), 1, m_entries.size()
        );
    }
    [[nodiscard]] std::size_t visibleIn(const InputContext& context) const {
        const std::optional<FloatRect> list = context.overlay();
        return list.has_value() ? visibleIn(list->height(), context.sizes())
                                : std::min(m_entries.size(), m_options.maxVisible);
    }

    /// Keeps the first shown entry such that the list is full, and, if asked, the highlight
    /// among the entries shown.
    void clampScroll(std::size_t shown, bool followHighlight) const {
        if (followHighlight) {
            if (m_highlight < m_first) {
                m_first = m_highlight;
            } else if (m_highlight >= m_first + shown) {
                m_first = m_highlight + 1 - shown;
            }
        }
        m_first = std::min(m_first, m_entries.size() - shown);
    }

    /// The entry under a point in the widget's coordinates, if it is over the open list.
    [[nodiscard]] std::optional<std::size_t> entryAt(sf::Vector2f at, const InputContext& context) const {
        const std::optional<FloatRect> list = context.overlay();
        if (!list.has_value() || !list->contains(at)) {
            return std::nullopt;
        }
        const float y = at.y - list->top() - listPadding(context.sizes());
        if (y < 0.f) {
            return std::nullopt;
        }
        const std::size_t row = static_cast<std::size_t>(y / entryHeight(context.sizes()));
        if (row >= visibleIn(context) || m_first + row >= m_entries.size()) {
            return std::nullopt;
        }
        return m_first + row;
    }

    void highlightAt(sf::Vector2f at, InputContext& context) {
        if (const std::optional<std::size_t> entry = entryAt(at, context); entry && *entry != m_highlight) {
            m_highlight = *entry;
            context.markDirty();
        }
    }

    void handleKey(sf::Keyboard::Key key, InputContext& context) {
        using Key = sf::Keyboard::Key;
        switch (key) {
            case Key::Up:
                m_highlight = m_highlight > 0 ? m_highlight - 1 : 0;
                break;
            case Key::Down:
                m_highlight = std::min(m_highlight + 1, m_entries.size() - 1);
                break;
            case Key::Home:
                m_highlight = 0;
                break;
            case Key::End:
                m_highlight = m_entries.size() - 1;
                break;
            case Key::Enter:
                choose(m_highlight, context);
                return;
            case Key::Escape:
                close(context);
                return;
            default:
                return;
        }
        clampScroll(visibleIn(context), true);
        context.markDirty();
    }

    void choose(std::size_t entry, InputContext& context) {
        if (entry != m_selected) {
            m_selected = entry;
            context.changeValue(Value(m_selected), true);
        }
        close(context);
    }

    void close(InputContext& context) {
        context.closeOverlay();
        context.releaseFocus();
        context.markDirty();
    }

    std::string m_label;
    std::vector<std::string> m_entries; ///< Never empty.
    DropdownOptions m_options;
    std::size_t m_selected;
    std::size_t m_highlight = 0;
    mutable std::size_t m_first = 0; ///< The first entry the open list shows.
};

} // namespace

Dropdown::Dropdown(
    std::string dropdownName, std::vector<std::string> dropdownEntries, DropdownOptions dropdownOptions
) :
    name(std::move(dropdownName)),
    entries(std::move(dropdownEntries)),
    options(std::move(dropdownOptions)) {}

Dropdown::Dropdown(
    std::string dropdownName,
    std::vector<std::string> dropdownEntries,
    IndexBinding& selected,
    DropdownOptions dropdownOptions
) :
    name(std::move(dropdownName)),
    entries(std::move(dropdownEntries)),
    options(std::move(dropdownOptions)),
    binding(AnyBinding(selected)) {}

std::unique_ptr<Widget> Dropdown::create() const {
    const std::string who = "dropdown \"" + name + "\"";
    if (entries.empty()) {
        throw SetupError(who + ": it needs at least one entry");
    }
    if (options.initial >= entries.size()) {
        throw SetupError(
            who + ": initial entry " + std::to_string(options.initial) + " is not one of its " +
            std::to_string(entries.size()) + " entries"
        );
    }
    if (options.maxVisible == 0) {
        throw SetupError(who + ": maxVisible must be at least 1");
    }
    return std::make_unique<DropdownWidget>(options.label.empty() ? name : options.label, entries, options);
}

} // namespace atpl
