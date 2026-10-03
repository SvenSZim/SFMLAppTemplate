#pragma once

#include <string>
#include <string_view>

namespace atpl::widgets {

// Text travels as UTF-8 (`std::string`); a text input edits it as code points, so that the
// cursor never stands inside a character.

/// The code points of UTF-8 text. A byte that does not start a valid sequence becomes U+FFFD.
[[nodiscard]] inline std::u32string decodeUtf8(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto byte = static_cast<unsigned char>(text[i]);
        const int length = byte < 0x80           ? 1
                           : (byte >> 5) == 0x6  ? 2
                           : (byte >> 4) == 0xE  ? 3
                           : (byte >> 3) == 0x1E ? 4
                                                 : 0;
        if (length == 0 || i + static_cast<std::size_t>(length) > text.size()) {
            result.push_back(U'\uFFFD');
            ++i;
            continue;
        }
        char32_t code = length == 1 ? byte : byte & (0x7F >> length);
        bool valid = true;
        for (int k = 1; k < length; ++k) {
            const auto next = static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]);
            valid = valid && (next >> 6) == 0x2;
            code = (code << 6) | (next & 0x3F);
        }
        if (!valid) {
            result.push_back(U'\uFFFD');
            ++i;
            continue;
        }
        result.push_back(code);
        i += static_cast<std::size_t>(length);
    }
    return result;
}

/// UTF-8 for code points.
[[nodiscard]] inline std::string encodeUtf8(std::u32string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char32_t code : text) {
        if (code < 0x80) {
            result.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (code >> 6)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (code >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (code >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    return result;
}

/// Whether a typed character is one to insert: not a control character.
[[nodiscard]] constexpr bool isPrintable(char32_t code) {
    return code >= 0x20 && code != 0x7F && !(code >= 0x80 && code < 0xA0) && code <= 0x10FFFF;
}

} // namespace atpl::widgets
