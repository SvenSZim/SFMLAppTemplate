#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/widgets/shapes_of_widgets.hpp"
#include "ui/widgets/utf8.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace atpl {

namespace {

/// A block of changing text below its label. Its height is fixed by how many lines it shows.
class TextDisplayWidget final : public Widget {
public:
    TextDisplayWidget(std::string label, TextDisplayOptions options) :
        m_label(std::move(label)),
        m_options(std::move(options)),
        m_text(m_options.initial) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const Sizes& sizes = context.sizes();
        const sf::Vector2f label = context.textSize(m_label, TextDisplay::Label);
        const float height =
            label.y + widgets::labelGap +
            static_cast<float>(m_options.lines) * lineHeight(context.textSize("Ag", TextDisplay::Text).y);
        return {
            .min = { sizes.rowHeight * 4.f, height },
            .preferred = { std::max(label.x, sizes.panelWidth - sizes.padding.x * 2.f), height },
            .max = sf::Vector2f(widgets::anyWidth, height), // fixed: the text never makes it higher
        };
    }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const PartStyle labelStyle = style.part(TextDisplay::Label);
        const float labelHeight = painter.textSize(m_label, labelStyle).y;
        painter.text(FloatRect(0.f, 0.f, size.x, labelHeight), m_label, labelStyle);

        const PartStyle text = style.part(TextDisplay::Text);
        const float line = lineHeight(painter.textSize("Ag", text).y);
        const FloatRect area = widgets::fieldBelow(size, labelHeight);
        const auto width = [&](std::string_view piece) { return painter.textSize(piece, text).x; };
        const std::vector<std::string> lines = wrap(m_text, area.width(), m_options.lines, width);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const FloatRect row(area.left(), area.top() + static_cast<float>(i) * line, area.width(), line);
            painter.text(row, lines[i], text, m_options.align); // the last one is cut with an ellipsis if too long
        }
    }

    // Shown only: it reacts to nothing and changes nothing.
    [[nodiscard]] bool reactsToPointer() const override { return false; }
    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == ValueKind::Text; }
    [[nodiscard]] std::optional<Value> value() const override { return Value(m_text); }

    void setValue(const Value& value) override {
        if (const auto* text = std::get_if<std::string>(&value)) {
            m_text = *text;
        }
    }

private:
    [[nodiscard]] static float lineHeight(float textHeight) { return std::max(std::round(textHeight * 1.3f), 1.f); }

    /// The text in at most `count` lines of `room`: words wrap, line breaks start new lines, and
    /// a word wider than a line is broken between its characters. What does not fit goes into
    /// the last line, to be cut there with an ellipsis.
    template <typename Width>
    [[nodiscard]] static std::vector<std::string>
    wrap(std::string_view text, float room, std::size_t count, const Width& width) {
        std::vector<std::string> lines;
        std::string current;
        bool fresh = true; // nothing on the current line yet
        std::size_t at = 0;
        const auto finish = [&] {
            lines.push_back(std::move(current));
            current.clear();
            fresh = true;
        };
        while (at < text.size() && lines.size() + 1 < count) {
            if (text[at] == '\n') {
                finish();
                ++at;
                continue;
            }
            if (text[at] == ' ' && fresh) {
                ++at; // no space at the start of a line
                continue;
            }
            const std::size_t end = std::min(text.find_first_of(" \n", at), text.size());
            const std::string_view word = text.substr(at, end - at);
            const std::string candidate = fresh ? std::string(word) : current + " " + std::string(word);
            if (width(candidate) <= room) {
                current = candidate;
                fresh = false;
                at = end;
                if (at < text.size() && text[at] == ' ') {
                    ++at;
                }
                continue;
            }
            if (!fresh) {
                finish(); // the word starts the next line
                continue;
            }
            // A word wider than a whole line: as many of its characters as fit.
            const std::u32string codes = widgets::decodeUtf8(word);
            std::size_t fit = 1;
            while (fit < codes.size() &&
                   width(widgets::encodeUtf8(std::u32string_view(codes).substr(0, fit + 1))) <= room) {
                ++fit;
            }
            const std::string head = widgets::encodeUtf8(std::u32string_view(codes).substr(0, fit));
            current = head;
            at += head.size();
            finish();
        }
        // The last line: whatever is left, on one line.
        std::string rest = current;
        if (at < text.size()) {
            std::string tail(text.substr(at));
            std::replace(tail.begin(), tail.end(), '\n', ' ');
            rest += fresh || rest.empty() ? tail : " " + tail;
        }
        if (!rest.empty() || lines.empty()) {
            lines.push_back(std::move(rest));
        }
        return lines;
    }

    std::string m_label;
    TextDisplayOptions m_options;
    std::string m_text;
};

} // namespace

TextDisplay::TextDisplay(std::string displayName, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)) {}

TextDisplay::TextDisplay(std::string displayName, Param<std::string>& text, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)),
    binding(AnyBinding(text)) {}

TextDisplay::TextDisplay(std::string displayName, TextBinding& text, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)),
    binding(AnyBinding(text)) {}

std::unique_ptr<Widget> TextDisplay::create() const {
    if (options.lines == 0) {
        throw SetupError("text display \"" + name + "\": it needs to show at least 1 line");
    }
    return std::make_unique<TextDisplayWidget>(options.label.empty() ? name : options.label, options);
}

} // namespace atpl
