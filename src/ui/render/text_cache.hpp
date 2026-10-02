#pragma once

#include "ui/render/draw_list.hpp"
#include "ui/render/text_renderer.hpp"

#include <SFML/Graphics/Text.hpp>

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace atpl::render {

/// Draws text, and keeps what it built for as long as the text stays the same (cache level 3).
///
/// Every text run of a layer has a text object that holds its glyphs. The object is built when
/// the run first appears and again only when the run changes: its string, font, size, colour,
/// room, alignment or wrapping. A panel that is repainted with the same text builds nothing,
/// and a panel that is not repainted is not even looked at.
///
/// Building a run means fitting it into its room first: one line of text that is too wide ends
/// in an ellipsis, wrapped text is broken into lines.
///
/// One draw call per run. (Building all glyphs of a layer into one batch would bring that down
/// to one per text size; see the decision log, Q7.)
class TextCache final : public TextRenderer {
public:
    std::size_t draw(sf::RenderTarget& target, const sf::RenderStates& states, const DrawList& layer) override;

    /// Drops what is kept for a layer that no longer exists.
    void forget(const DrawList& layer);

    /// How many text objects have been built or rebuilt so far. For tests and for the profiler.
    [[nodiscard]] std::size_t buildCount() const { return m_buildCount; }

private:
    struct Entry {
        bool built = false;
        TextRun run;                  // what the text object was built from
        std::optional<sf::Text> text; // empty for a run without a font
    };
    struct Layer {
        std::size_t revision = 0; // the layer's text revision the entries match
        bool seen = false;
        std::vector<Entry> entries;
    };

    void update(Layer& cached, const DrawList& layer);
    void build(Entry& entry, const TextRun& run);

    std::unordered_map<const DrawList*, Layer> m_layers;
    std::size_t m_buildCount = 0;
};

} // namespace atpl::render
