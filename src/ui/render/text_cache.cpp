#include "ui/render/text_cache.hpp"

#include "ui/render/font_measurer.hpp"
#include "ui/render/text_layout.hpp"

#include <SFML/System/String.hpp>

#include <cmath>

namespace atpl::render {

namespace {

[[nodiscard]] bool sameRun(const TextRun& a, const TextRun& b) {
    return a.text == b.text && a.font == b.font && a.size == b.size && a.color == b.color && a.rect == b.rect &&
           a.align == b.align && a.wrapped == b.wrapped;
}

/// The run's text as it is shown: cut short with an ellipsis, or broken into lines.
[[nodiscard]] sf::String fitted(const TextRun& run, const Advance& advance, float& widestLine) {
    const std::u32string text = decodeUtf8(run.text);

    if (!run.wrapped) {
        // Not every font has the ellipsis character; three dots do the same job.
        const std::u32string_view ellipsis = run.font->hasGlyph(U'\u2026') ? U"\u2026" : U"...";
        const std::u32string line = elide(text, run.rect.width(), advance, ellipsis);
        widestLine = lineWidth(line, advance);
        return sf::String(line);
    }

    std::u32string joined;
    widestLine = 0.f;
    for (const std::u32string& line : wrapLines(text, run.rect.width(), advance)) {
        if (!joined.empty()) {
            joined += U'\n';
        }
        joined += line;
        widestLine = std::max(widestLine, lineWidth(line, advance));
    }
    return sf::String(joined);
}

} // namespace

std::size_t TextCache::draw(sf::RenderTarget& target, const sf::RenderStates& states, const DrawList& layer) {
    Layer& cached = m_layers[&layer];
    if (!cached.seen || cached.revision != layer.textRevision()) {
        update(cached, layer);
        cached.seen = true;
        cached.revision = layer.textRevision();
    }

    std::size_t drawCalls = 0;
    for (const Entry& entry : cached.entries) {
        if (entry.text.has_value()) {
            target.draw(*entry.text, states);
            ++drawCalls;
        }
    }
    return drawCalls;
}

void TextCache::forget(const DrawList& layer) {
    m_layers.erase(&layer);
}

void TextCache::update(Layer& cached, const DrawList& layer) {
    const auto runs = layer.texts();
    cached.entries.resize(runs.size());

    // Runs are matched by their place in the list: a panel paints its text in the same order
    // every time, so a run that did not change is found where it was.
    for (std::size_t i = 0; i < runs.size(); ++i) {
        Entry& entry = cached.entries[i];
        if (!entry.built || !sameRun(entry.run, runs[i])) {
            build(entry, runs[i]);
        }
    }
}

void TextCache::build(Entry& entry, const TextRun& run) {
    entry.built = true;
    entry.run = run;
    entry.text.reset();
    if (run.font == nullptr) {
        return;
    }
    ++m_buildCount;

    const Advance advance = advanceOf(*run.font, run.size);
    float width = 0.f;
    const sf::String shown = fitted(run, advance, width);

    sf::Text text(*run.font, shown, characterSize(run.size));
    text.setFillColor(run.color);

    // Horizontally as the alignment says.
    float x = run.rect.left();
    if (run.align == Align::Center) {
        x += (run.rect.width() - width) * 0.5f;
    } else if (run.align == Align::Right) {
        x += run.rect.width() - width;
    }

    // Vertically, wrapped text starts at the top of its room. One line is centred by the height
    // of the font's capital letters, not by what the string happens to contain: text of one size
    // then sits on the same baseline everywhere, and looks centred in a button.
    // (SFML puts the baseline one character size below a text's position.)
    const unsigned int pixels = characterSize(run.size);
    float y = run.rect.top();
    if (!run.wrapped) {
        const float capitalHeight = -run.font->getGlyph(U'H', pixels, false).bounds.position.y;
        const float baseline = run.rect.top() + (run.rect.height() + capitalHeight) * 0.5f;
        y = baseline - static_cast<float>(pixels);
    }

    // Whole pixels keep glyphs sharp.
    text.setPosition({ std::round(x), std::round(y) });
    entry.text.emplace(std::move(text));
}

} // namespace atpl::render
