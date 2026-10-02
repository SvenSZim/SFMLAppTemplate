#include "ui/render/text_layout.hpp"

#include <cstdint>

namespace atpl::render {

namespace {

constexpr char32_t replacement = U'\uFFFD';

} // namespace

std::u32string decodeUtf8(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());

    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[i]);

        // How many bytes this character has, and the bits its first byte contributes.
        int length = 0;
        char32_t codePoint = 0;
        if (lead < 0x80) {
            length = 1;
            codePoint = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            length = 2;
            codePoint = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            codePoint = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            codePoint = lead & 0x07;
        }

        bool valid = length > 0 && i + static_cast<std::size_t>(length) <= text.size();
        for (int k = 1; valid && k < length; ++k) {
            const auto next = static_cast<std::uint8_t>(text[i + static_cast<std::size_t>(k)]);
            if ((next & 0xC0) != 0x80) {
                valid = false;
            } else {
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
        }

        if (valid) {
            result.push_back(codePoint);
            i += static_cast<std::size_t>(length);
        } else {
            result.push_back(replacement);
            ++i;
        }
    }
    return result;
}

float lineWidth(std::u32string_view text, const Advance& advance) {
    float width = 0.f;
    char32_t previous = 0;
    for (const char32_t current : text) {
        width += advance(previous, current);
        previous = current;
    }
    return width;
}

std::u32string elide(std::u32string_view text, float maxWidth, const Advance& advance, std::u32string_view ellipsis) {
    if (lineWidth(text, advance) <= maxWidth) {
        return std::u32string(text);
    }

    const float ellipsisWidth = lineWidth(ellipsis, advance);
    if (ellipsisWidth > maxWidth) {
        return {};
    }

    // The longest start of the text that leaves room for the ellipsis.
    std::size_t kept = 0;
    float width = 0.f;
    char32_t previous = 0;
    for (const char32_t current : text) {
        const float next = width + advance(previous, current);
        if (next + ellipsisWidth > maxWidth) {
            break;
        }
        width = next;
        previous = current;
        ++kept;
    }
    while (kept > 0 && text[kept - 1] == U' ') {
        --kept;
    }

    std::u32string result(text.substr(0, kept));
    result += ellipsis;
    return result;
}

std::vector<std::u32string> wrapLines(std::u32string_view text, float maxWidth, const Advance& advance) {
    std::vector<std::u32string> lines;
    std::u32string line;
    float lineWidthSoFar = 0.f;
    char32_t previous = 0;

    // Where the current line could be broken: after the last space seen.
    std::size_t breakAt = std::u32string::npos; // length of `line` up to and excluding that space

    const auto finishLine = [&] {
        lines.push_back(std::move(line));
        line.clear();
        lineWidthSoFar = 0.f;
        previous = 0;
        breakAt = std::u32string::npos;
    };

    for (const char32_t current : text) {
        if (current == U'\n') {
            finishLine();
            continue;
        }

        const float step = advance(previous, current);
        if (lineWidthSoFar + step > maxWidth && !line.empty()) {
            if (current == U' ') {
                // The space itself is where the line ends.
                finishLine();
                continue;
            }
            if (breakAt != std::u32string::npos) {
                // Move what came after the last space to the next line.
                std::u32string rest = line.substr(breakAt + 1);
                line.resize(breakAt);
                lines.push_back(std::move(line));
                line = std::move(rest);
                lineWidthSoFar = render::lineWidth(line, advance);
                previous = line.empty() ? char32_t{ 0 } : line.back();
                breakAt = std::u32string::npos;
            } else {
                // One word wider than the line: break it here.
                finishLine();
            }
        }

        if (current == U' ') {
            breakAt = line.size();
        }
        lineWidthSoFar += advance(previous, current);
        line.push_back(current);
        previous = current;
    }

    lines.push_back(std::move(line));
    return lines;
}

} // namespace atpl::render
