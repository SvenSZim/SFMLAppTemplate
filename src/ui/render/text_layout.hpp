#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace atpl::render {

// Fitting text into the room it has: measuring a line, cutting it short, and wrapping it.
//
// These functions know nothing about fonts. They ask how far the pen moves from one character
// to the next through an `Advance` function, so the rules can be tested with made-up widths.

/// How far the pen moves for `current`, given the character before it (0 at the start of a line).
/// Includes kerning.
using Advance = std::function<float(char32_t previous, char32_t current)>;

/// UTF-8 text as code points. Bytes that are not valid UTF-8 become U+FFFD.
[[nodiscard]] std::u32string decodeUtf8(std::string_view text);

/// The width of one line of text.
[[nodiscard]] float lineWidth(std::u32string_view text, const Advance& advance);

/// The text itself if it fits into `maxWidth`; otherwise as much of its start as fits together
/// with `ellipsis`, followed by the ellipsis. Trailing spaces before the ellipsis are dropped.
/// If not even the ellipsis fits, the result is empty.
[[nodiscard]] std::u32string
elide(std::u32string_view text, float maxWidth, const Advance& advance, std::u32string_view ellipsis = U"…");

/// The text broken into lines no wider than `maxWidth`.
///
/// Lines break at spaces; the space at a break is dropped. A word that is wider than a whole
/// line is broken where the line is full. A line break in the text starts a new line. There is
/// always at least one line, which may be empty.
[[nodiscard]] std::vector<std::u32string> wrapLines(std::u32string_view text, float maxWidth, const Advance& advance);

} // namespace atpl::render
