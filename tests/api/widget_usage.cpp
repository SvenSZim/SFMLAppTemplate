// An application's own widget type, and a customised theme.
//
// Compiled with every build, never linked or run. See core_usage.cpp for why the namespace is named.

#include "atpl/ui/ui.hpp"
#include "atpl/ui/widget.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace widget_usage {

using namespace atpl;

// ---------------------------------------------------------------------------------------------
// A widget the template does not have: a row of buttons of which one is selected.
// Its value is the index of the selected segment, so it can be bound to an enum like a dropdown.
// ---------------------------------------------------------------------------------------------

// The descriptor: the public face of the widget. It names the parts, for theming.
struct SegmentedControl {
    static constexpr Kind kind{ "segmented_control" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Selection{ kind, "selection", Role::Accent };
    static constexpr Part SegmentLabel{ kind, "segment_label", Role::Text };
    static constexpr Part Separators{ kind, "separators", Role::Line, Shown::No }; // optional part

    std::string name;
    std::vector<std::string> segments;

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

// The implementation: behaviour only. No positions, no colours, no draw calls.
class SegmentedControlWidget final : public Widget {
public:
    explicit SegmentedControlWidget(std::vector<std::string> segments) :
        m_segments(std::move(segments)) {}

    // Size: one row. The width is not the widget's to decide.
    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        return { .height = context.metrics().rowHeight };
    }

    // Input: a click selects the segment under the pointer.
    bool handleInput(const Event& event, InputContext& context) override {
        const auto* press = event.getIf<PointerPressed>();
        if (press == nullptr || press->button != sf::Mouse::Button::Left || m_segments.empty()) {
            return false;
        }

        const float segmentWidth = context.size().x / static_cast<float>(m_segments.size());
        const auto clicked = static_cast<std::size_t>(context.local(press->pointer).x / segmentWidth);
        const std::size_t index = std::min(clicked, m_segments.size() - 1);

        if (index != m_selected) {
            m_selected = index;
            m_highlight = 0.f; // restart the selection animation
            context.markDirty();
            context.changeValue(Value{ index }); // the UI writes the binding and raises ValueChanged
        }
        return true;
    }

    // Time: fade the selection in. Nothing happens, and nothing is redrawn, once it is done.
    void update(float dt, UpdateContext& context) override {
        if (m_highlight < 1.f) {
            m_highlight = std::min(1.f, m_highlight + dt * 6.f);
            context.markDirty();
        }
    }

    // Drawing: where the parts are. How they look comes from the style.
    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        painter.box(FloatRect({ 0.f, 0.f }, size), style.part(SegmentedControl::Track));
        if (m_segments.empty()) {
            return;
        }

        const float segmentWidth = size.x / static_cast<float>(m_segments.size());
        const auto segmentRect = [&](std::size_t index) {
            return FloatRect(static_cast<float>(index) * segmentWidth, 0.f, segmentWidth, size.y);
        };

        PartStyle selection = style.part(SegmentedControl::Selection, State::Active);
        selection.color.a = static_cast<std::uint8_t>(static_cast<float>(selection.color.a) * m_highlight);
        painter.box(segmentRect(m_selected).inset(2.f), selection);

        for (std::size_t i = 0; i < m_segments.size(); ++i) {
            const State state = i == m_selected ? State::Active : State::Normal;
            painter.text(
                segmentRect(i), m_segments[i], style.part(SegmentedControl::SegmentLabel, state), Align::Center
            );

            // An optional part: drawn only if the theme switches it on. No check needed.
            if (i > 0) {
                const float x = static_cast<float>(i) * segmentWidth;
                painter.line({ x, 4.f }, { x, size.y - 4.f }, style.part(SegmentedControl::Separators));
            }
        }
    }

    // Value: the selected index. The widget never sees what it is bound to.
    [[nodiscard]] bool accepts(ValueKind valueKind) const override { return valueKind == ValueKind::Index; }
    [[nodiscard]] bool editsValue() const override { return true; }
    [[nodiscard]] std::optional<Value> value() const override { return Value{ m_selected }; }
    void setValue(const Value& value) override {
        if (const auto* index = std::get_if<std::size_t>(&value); index != nullptr && !m_segments.empty()) {
            m_selected = std::min(*index, m_segments.size() - 1);
        }
    }

private:
    std::vector<std::string> m_segments;
    std::size_t m_selected = 0;
    float m_highlight = 1.f;
};

std::unique_ptr<Widget> SegmentedControl::create() const {
    return std::make_unique<SegmentedControlWidget>(segments);
}

// ---------------------------------------------------------------------------------------------
// Using it, and theming.
// ---------------------------------------------------------------------------------------------

enum class Algorithm { BreadthFirst, Dijkstra, AStar };

Theme makeTheme() {
    // Layer 1, tokens: a theme is complete with these alone.
    Theme theme = themes::moon();
    theme.palette.accents[0].accent = sf::Color(212, 163, 115);
    theme.shape.radius = 14.f;
    theme.metrics.rowHeight = 30.f;
    theme.metrics.scale = 1.25f;

    // Text types: a size and a font each. Titles get their own font here.
    const auto titleFont = std::make_shared<sf::Font>();
    theme.typography.title = { .size = 18.f, .font = titleFont };
    theme.typography.muted.size = 11.f;

    // Layer 3, part entries: for one part of one kind of widget.
    theme[Slider::Ticks].shown = true;              // an optional part of a built-in widget
    theme[Panel::Background].borderThickness = 2.f; // a thicker panel outline
    theme[Button::Face].radius = 0.f;               // sharp buttons, everything else stays round
    theme[Switch::Knob].radius = fullyRound;        // a circle
    theme[Button::Label].font = titleFont;          // one part with another font

    // The application's own widget is themed the same way. Without these two lines it would
    // still look right: its parts get their look from their roles (layer 2).
    theme[SegmentedControl::Separators].shown = true;
    theme[SegmentedControl::Selection].radius = 6.f;

    return theme;
}

UISetup makeSetup() {
    return {
        .theme = makeTheme(),
        .panels = {
            {
                .name = "Search",
                .accent = 0, // which of the theme's accent colours this panel uses
                .widgets = {
                    Paragraph("Intro", {.heading = "Path search", .text = "Pick an algorithm and watch it explore."}),
                    // Listed like any built-in widget.
                    SegmentedControl{ .name = "Algorithm", .segments = { "BFS", "Dijkstra", "A*" } },
                    Slider("Speed", {.min = 1.0, .max = 60.0}),
                    // One widget in other colours than its panel.
                    colored({.accent = 0}, Button("Clear walls")),
                    Paragraph("Hint", {.footer = "Right-click places a wall."}),
                },
            },
        },
    };
}

void link(UI& ui, Param<Algorithm>& algorithm) {
    // Bound like any built-in widget: an enum is an index.
    ui.widget("Algorithm").bind(algorithm);

    // Themes can be changed while running.
    Theme lighter = ui.theme();
    lighter.palette.mains[0] = sf::Color(60, 60, 64); // the first main colour: panel backgrounds
    ui.setTheme(lighter);
}

} // namespace widget_usage
