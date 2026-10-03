#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace atpl;
using Catch::Approx;

namespace {

/// Text of a fixed width per character that wraps at spaces, so tests need no font: every
/// character is half as wide as the text is high, and a line is one text height.
class WrappingText final : public render::TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font*, float size) const override {
        return { static_cast<float>(text.size()) * size * 0.5f, size };
    }
    [[nodiscard]] float wrappedHeight(std::string_view text, const sf::Font*, float size, float width) const override {
        return static_cast<float>(lines(text, size * 0.5f, width)) * size;
    }

    /// Lines of words, greedily filled; explicit line breaks start new lines.
    static int lines(std::string_view text, float character, float width) {
        const auto fit = std::max(static_cast<int>(width / character), 1);
        int count = 0;
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t end = std::min(text.find('\n', start), text.size());
            const std::string_view line = text.substr(start, end - start);
            int used = 0;
            int lineCount = 1;
            std::size_t wordStart = 0;
            while (wordStart <= line.size()) {
                const std::size_t wordEnd = std::min(line.find(' ', wordStart), line.size());
                const auto word = static_cast<int>(wordEnd - wordStart);
                const int needed = used == 0 ? word : used + 1 + word;
                if (needed > fit && used > 0) {
                    ++lineCount;
                    used = word;
                } else {
                    used = needed;
                }
                lineCount += std::max(used - 1, 0) / fit; // a word longer than a line is broken
                used = used > fit ? used % fit : used;
                wordStart = wordEnd + 1;
            }
            count += lineCount;
            start = end + 1;
        }
        return count;
    }
};

const WrappingText measurer;
const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f }); // nothing scaled
const Theme theme;

[[nodiscard]] float lineOf(const Part& part) {
    return theme.resolve(part).textSize;
}

[[nodiscard]] SizeRequest measured(const ParagraphOptions& options, float width) {
    const auto widget = Paragraph("P", options).create();
    return widget->measure(MeasureContext(width, theme, {}, sizes, &measurer));
}

[[nodiscard]] render::DrawList painted(const ParagraphOptions& options, sf::Vector2f size, const Theme& look = theme) {
    const auto widget = Paragraph("P", options).create();
    render::DrawList list;
    Painter painter(list, { 0.f, 0.f }, size, &measurer);
    widget->paint(painter, Style(look, {}, State::Normal, sizes));
    return list;
}

} // namespace

TEST_CASE(
    "a paragraph is as high as its texts, with a gap between each two, and empty ones take no room",
    "[ui][widgets][paragraph]"
) {
    const float heading = lineOf(Paragraph::Heading);
    const float body = lineOf(Paragraph::Body);
    const float footer = lineOf(Paragraph::Footer);
    const float gap = sizes.gap.y;

    // Every combination of the three, each short enough for one line.
    for (int mask = 0; mask < 8; ++mask) {
        ParagraphOptions options;
        float expected = 0.f;
        int texts = 0;
        if ((mask & 1) != 0) {
            options.heading = "Heading";
            expected += heading;
            ++texts;
        }
        if ((mask & 2) != 0) {
            options.text = "Body";
            expected += body;
            ++texts;
        }
        if ((mask & 4) != 0) {
            options.footer = "Footer";
            expected += footer;
            ++texts;
        }
        expected += gap * static_cast<float>(std::max(texts - 1, 0));
        INFO("texts " << mask);
        const SizeRequest request = measured(options, 300.f);
        REQUIRE(request.min.y == Approx(expected));
        REQUIRE(request.preferred.y == Approx(expected));
        REQUIRE(request.isDynamic());
    }
}

TEST_CASE("a paragraph wraps all three texts again for a different width", "[ui][widgets][paragraph]") {
    const ParagraphOptions options{
        .heading = "A heading that is rather long",
        .text = "Some body text that goes on for a while, longer than one line of a narrow panel.",
        .footer = "And a footer below it",
    };
    const float wide = measured(options, 2000.f).min.y;
    const float narrow = measured(options, 120.f).min.y;
    const float narrower = measured(options, 60.f).min.y;
    REQUIRE(narrow > wide);
    REQUIRE(narrower > narrow);

    // The heading wraps too.
    const float headingOnly = measured({ .heading = "A heading that is rather long" }, 60.f).min.y;
    REQUIRE(headingOnly > lineOf(Paragraph::Heading));

    // A line break starts a new line.
    REQUIRE(measured({ .text = "one\ntwo" }, 2000.f).min.y == Approx(lineOf(Paragraph::Body) * 2.f));
}

TEST_CASE(
    "a paragraph would like its longest line, never more than a panel, and needs its longest word",
    "[ui][widgets][paragraph]"
) {
    const float character = lineOf(Paragraph::Body) * 0.5f;
    const SizeRequest shortText = measured({ .text = "short words\nand a longer line" }, 300.f);
    REQUIRE(shortText.preferred.x == Approx(17.f * character)); // "and a longer line"
    REQUIRE(shortText.min.x == Approx(6.f * character));        // "longer"

    std::string longText;
    for (int i = 0; i < 100; ++i) {
        longText += "word ";
    }
    const SizeRequest request = measured({ .text = longText }, 300.f);
    REQUIRE(request.preferred.x == Approx(sizes.panelWidth - sizes.padding.x * 2.f)); // it wraps instead

    const SizeRequest empty = measured({}, 300.f);
    REQUIRE(empty.min == sf::Vector2f());
    REQUIRE(empty.preferred == sf::Vector2f());
}

TEST_CASE(
    "a paragraph paints its texts one below the other, each in its own part, aligned as asked",
    "[ui][widgets][paragraph]"
) {
    const render::DrawList list = painted(
        { .heading = "Ants", .text = "They find food.", .footer = "v1.5", .align = Align::Center }, { 300.f, 200.f }
    );
    const auto runs = list.texts();
    REQUIRE(runs.size() == 3);
    REQUIRE(runs[0].text == "Ants");
    REQUIRE(runs[1].text == "They find food.");
    REQUIRE(runs[2].text == "v1.5");
    for (const render::TextRun& run : runs) {
        REQUIRE(run.wrapped);
        REQUIRE(run.align == Align::Center);
    }
    REQUIRE(runs[0].size == lineOf(Paragraph::Heading));
    REQUIRE(runs[2].color == theme.resolve(Paragraph::Footer).color);
    REQUIRE(runs[1].rect.top() == Approx(runs[0].rect.bottom() + sizes.gap.y));
    REQUIRE(runs[2].rect.top() == Approx(runs[1].rect.bottom() + sizes.gap.y));
}

TEST_CASE("lines between the texts show only if the theme shows them, and change no size", "[ui][widgets][paragraph]") {
    const ParagraphOptions options{ .heading = "Ants", .text = "They find food.", .footer = "v1.5" };
    REQUIRE(painted(options, { 300.f, 200.f }).shapes().empty());

    Theme lined;
    lined[Paragraph::Separator].shown = true;
    const render::DrawList list = painted(options, { 300.f, 200.f }, lined);
    REQUIRE_FALSE(list.shapes().empty());
    const sf::Color line = lined.resolve(Paragraph::Separator).color;
    REQUIRE(std::all_of(list.shapes().begin(), list.shapes().end(), [&](const sf::Vertex& v) {
        return v.color == line;
    }));
    REQUIRE(list.texts()[1].rect.top() == Approx(painted(options, { 300.f, 200.f }).texts()[1].rect.top()));

    // A heading alone has nothing to separate.
    REQUIRE(painted({ .heading = "Ants" }, { 300.f, 200.f }, lined).shapes().empty());
}

TEST_CASE("a paragraph is static: it takes no value and does not react to the pointer", "[ui][widgets][paragraph]") {
    const auto widget = Paragraph("P", { .text = "Hello" }).create();
    REQUIRE_FALSE(widget->accepts(ValueKind::Text));
    REQUIRE_FALSE(widget->value().has_value());
    REQUIRE_FALSE(widget->reactsToPointer());
}
