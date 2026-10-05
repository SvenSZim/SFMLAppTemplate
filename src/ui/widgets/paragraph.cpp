#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// Up to three texts one below the other: heading, body, footer. Each wraps to the widget's
/// width; the ones that are empty take no room. The gap between two is the same whether a theme
/// shows a line in it or not, so showing the lines changes no size.
class ParagraphWidget final : public Widget {
public:
    explicit ParagraphWidget(ParagraphOptions options) :
        m_align(options.align),
        m_underline(options.underline && !options.heading.empty()) {
        const std::array<std::pair<const Part*, std::string*>, 3> all{ { { &Paragraph::Heading, &options.heading },
                                                                         { &Paragraph::Body, &options.text },
                                                                         { &Paragraph::Footer, &options.footer } } };
        for (const auto& [part, text] : all) {
            if (!text->empty()) {
                m_sections.push_back({ part, std::move(*text) });
            }
        }
    }

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        // As wide as its longest line, but never wider than a panel of the layout's width: a long
        // text wraps instead of widening its panel. At least as wide as its longest word.
        const float cap = std::max(sizes.panelWidth - sizes.padding.x * 2.f, 0.f);
        float longestLine = 0.f;
        float longestWord = 0.f;
        float height = 0.f;
        for (const Section& section : m_sections) {
            forEachPiece(section.text, "\n", [&](std::string_view line) {
                longestLine = std::max(longestLine, context.textSize(line, *section.part).x);
                forEachPiece(line, " ", [&](std::string_view word) {
                    longestWord = std::max(longestWord, context.textSize(word, *section.part).x);
                });
            });
            height += context.wrappedTextHeight(section.text, *section.part, context.width());
        }
        height += gapOf(sizes) * static_cast<float>(gaps());
        if (m_underline && m_sections.size() == 1) {
            height += gapOf(sizes) * 0.5f; // room for the line below a heading that stands alone
        }
        const float preferred = std::min(longestLine, cap);
        return {
            .min = { std::min(longestWord, preferred), height },
            .preferred = { preferred, height },
            .max = std::nullopt, // dynamic: its height follows its width
        };
    }

    void paint(Painter& painter, const Style& style) const override {
        const float width = painter.size().x;
        const float gap = gapOf(style.sizes());
        const PartStyle separator = style.part(Paragraph::Separator);
        const PartStyle underline = style.part(Paragraph::Underline);
        float y = 0.f;
        for (std::size_t i = 0; i < m_sections.size(); ++i) {
            const PartStyle part = style.part(*m_sections[i].part);
            const float height = painter.wrappedTextHeight(m_sections[i].text, part, width);
            painter.wrappedText(FloatRect(0.f, y, width, height), m_sections[i].text, part, m_align);
            y += height;
            if (i == 0 && m_underline) {
                // Below the heading, a little closer to it than to what follows.
                const float below = std::round(y + gap * 0.35f);
                painter.line({ 0.f, below }, { width, below }, underline);
            }
            if (i + 1 < m_sections.size()) {
                if (separator.shown && !(i == 0 && m_underline)) {
                    const float middle = std::round(y + gap * 0.5f);
                    painter.line({ 0.f, middle }, { width, middle }, separator);
                }
                y += gap;
            }
        }
    }

    // Static text: nothing to bind, nothing to operate.
    [[nodiscard]] bool reactsToPointer() const override { return false; }

private:
    struct Section {
        const Part* part;
        std::string text;
    };

    /// The space between two texts.
    [[nodiscard]] static float gapOf(const Sizes& sizes) { return sizes.gap.y; }

    [[nodiscard]] std::size_t gaps() const { return m_sections.empty() ? 0 : m_sections.size() - 1; }

    /// Calls `use` for each piece of `text` between the separators.
    template <typename Use>
    static void forEachPiece(std::string_view text, std::string_view separator, const Use& use) {
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t end = std::min(text.find(separator, start), text.size());
            use(text.substr(start, end - start));
            start = end + separator.size();
        }
    }

    std::vector<Section> m_sections; ///< The texts that are not empty, in order.
    Align m_align;
    bool m_underline = false; ///< A line below the heading.
};

} // namespace

Paragraph::Paragraph(std::string paragraphName, ParagraphOptions paragraphOptions) :
    name(std::move(paragraphName)),
    options(std::move(paragraphOptions)) {}

std::unique_ptr<Widget> Paragraph::create() const {
    return std::make_unique<ParagraphWidget>(options);
}

} // namespace atpl
