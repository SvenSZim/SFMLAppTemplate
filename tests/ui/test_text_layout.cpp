#include "ui/render/text_layout.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using atpl::render::Advance;
using atpl::render::decodeUtf8;
using atpl::render::elide;
using atpl::render::lineWidth;
using atpl::render::wrapLines;
using Catch::Approx;

namespace {

// Every character is 10 wide: widths in the tests are easy to follow.
const Advance fixed = [](char32_t, char32_t) { return 10.f; };

// Narrow letters and kerning, to make sure both are taken from the function.
const Advance uneven = [](char32_t previous, char32_t current) {
    const float width = current == U'i' ? 4.f : 10.f;
    const float kerning = (previous == U'A' && current == U'V') ? -3.f : 0.f;
    return width + kerning;
};

using Lines = std::vector<std::u32string>;

} // namespace

// ----- Decoding -----

TEST_CASE("plain text decodes to its characters", "[ui][text]") {
    REQUIRE(decodeUtf8("abc") == U"abc");
    REQUIRE(decodeUtf8("").empty());
}

TEST_CASE("characters of two, three and four bytes decode to one code point each", "[ui][text]") {
    REQUIRE(decodeUtf8("\xC3\xA4") == U"\u00E4");             // a-umlaut
    REQUIRE(decodeUtf8("\xE2\x80\xA6") == U"\u2026");         // the ellipsis
    REQUIRE(decodeUtf8("\xF0\x9F\x98\x80") == U"\U0001F600"); // an emoji
    REQUIRE(decodeUtf8("a\xC3\xA4z") == U"a\u00E4z");
}

TEST_CASE("bytes that are not valid text become the replacement character", "[ui][text]") {
    REQUIRE(decodeUtf8("a\xFFz") == U"a\uFFFDz");
    REQUIRE(decodeUtf8("\xC3") == U"\uFFFD");   // cut off in the middle of a character
    REQUIRE(decodeUtf8("\xC3(") == U"\uFFFD("); // a start byte followed by something else
    REQUIRE(decodeUtf8("\x80").size() == 1);    // a continuation byte on its own
}

// ----- Measuring -----

TEST_CASE("a line is as wide as its characters together", "[ui][text]") {
    REQUIRE(lineWidth(U"", fixed) == 0.f);
    REQUIRE(lineWidth(U"abcd", fixed) == 40.f);
    REQUIRE(lineWidth(U"hi", uneven) == 14.f);
    REQUIRE(lineWidth(U"AV", uneven) == 17.f); // kerning pulls the pair together
}

// ----- Cutting short -----

TEST_CASE("text that fits is left alone", "[ui][text]") {
    REQUIRE(elide(U"hello", 50.f, fixed) == U"hello");
    REQUIRE(elide(U"hello", 500.f, fixed) == U"hello");
    REQUIRE(elide(U"", 0.f, fixed).empty());
}

TEST_CASE("text that is too wide ends in an ellipsis and then fits", "[ui][text]") {
    const std::u32string result = elide(U"hello world", 60.f, fixed);

    REQUIRE(result == U"hello\u2026"); // five characters and the ellipsis: 60
    REQUIRE(lineWidth(result, fixed) <= 60.f);
}

TEST_CASE("no space is left hanging before the ellipsis", "[ui][text]") {
    REQUIRE(elide(U"hello world", 70.f, fixed) == U"hello\u2026"); // no space between "hello" and the ellipsis
}

TEST_CASE("the ellipsis can be given, for fonts that lack the character", "[ui][text]") {
    const std::u32string result = elide(U"hello world", 60.f, fixed, U"...");

    REQUIRE(result == U"hel...");
    REQUIRE(lineWidth(result, fixed) <= 60.f);
}

TEST_CASE("where not even the ellipsis fits, nothing is shown", "[ui][text]") {
    REQUIRE(elide(U"hello", 5.f, fixed).empty());
    REQUIRE(elide(U"hello", 10.f, fixed) == U"\u2026"); // room for the ellipsis alone
}

// ----- Wrapping -----

TEST_CASE("text that fits stays on one line", "[ui][text]") {
    REQUIRE(wrapLines(U"hello world", 200.f, fixed) == Lines{ U"hello world" });
    REQUIRE(wrapLines(U"", 200.f, fixed) == Lines{ U"" });
}

TEST_CASE("lines break at spaces, and the space at the break is dropped", "[ui][text]") {
    // 60 wide: six characters per line.
    REQUIRE(wrapLines(U"hello world", 60.f, fixed) == Lines{ U"hello", U"world" });
    REQUIRE(wrapLines(U"one two three four", 80.f, fixed) == Lines{ U"one two", U"three", U"four" });
}

TEST_CASE("as many words as fit share a line", "[ui][text]") {
    REQUIRE(wrapLines(U"a bb ccc dddd", 60.f, fixed) == Lines{ U"a bb", U"ccc", U"dddd" });
    REQUIRE(wrapLines(U"a bb ccc dddd", 80.f, fixed) == Lines{ U"a bb ccc", U"dddd" });
}

TEST_CASE("a word wider than a line is broken where the line is full", "[ui][text]") {
    REQUIRE(wrapLines(U"abcdefghij", 40.f, fixed) == Lines{ U"abcd", U"efgh", U"ij" });
    REQUIRE(wrapLines(U"hi abcdefgh", 40.f, fixed) == Lines{ U"hi", U"abcd", U"efgh" });
}

TEST_CASE("a line break in the text starts a new line", "[ui][text]") {
    REQUIRE(wrapLines(U"one\ntwo", 200.f, fixed) == Lines{ U"one", U"two" });
    REQUIRE(wrapLines(U"one\n\ntwo", 200.f, fixed) == Lines{ U"one", U"", U"two" });
    REQUIRE(wrapLines(U"hello world\nagain", 60.f, fixed) == Lines{ U"hello", U"world", U"again" });
}

TEST_CASE("every wrapped line fits, whatever the widths", "[ui][text]") {
    const std::u32string text = U"this is a little longer text with iii narrow and WIDE words in it";

    for (const float width : { 30.f, 55.f, 80.f, 123.f, 400.f }) {
        const Lines lines = wrapLines(text, width, uneven);
        std::u32string rejoined;
        for (const std::u32string& line : lines) {
            REQUIRE(lineWidth(line, uneven) <= width);
            rejoined += line;
        }
        // Nothing but the spaces at the breaks is lost.
        std::u32string withoutSpaces;
        for (const char32_t c : text) {
            if (c != U' ') {
                withoutSpaces += c;
            }
        }
        std::u32string rejoinedWithoutSpaces;
        for (const char32_t c : rejoined) {
            if (c != U' ') {
                rejoinedWithoutSpaces += c;
            }
        }
        REQUIRE(rejoinedWithoutSpaces == withoutSpaces);
    }
}
