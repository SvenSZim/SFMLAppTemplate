#include "atpl/core/text_log.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <thread>
#include <vector>

using namespace atpl;

namespace {

std::vector<std::string> textsOf(const std::vector<LogLine>& lines) {
    std::vector<std::string> texts;
    for (const LogLine& line : lines) {
        texts.push_back(line.text);
    }
    return texts;
}

} // namespace

TEST_CASE("a text log keeps the newest lines, oldest first", "[core][text_log]") {
    TextLog log(3);
    REQUIRE(log.capacity() == 3);
    REQUIRE(log.size() == 0);
    for (const char* text : { "a", "b", "c", "d" }) {
        log.push(text);
    }
    std::vector<LogLine> lines;
    REQUIRE(log.read(lines, 10) == 3);
    REQUIRE(textsOf(lines) == std::vector<std::string>{ "b", "c", "d" }); // "a" was dropped
    REQUIRE(log.read(lines, 2) == 2);
    REQUIRE(textsOf(lines) == std::vector<std::string>{ "c", "d" }); // the newest two
    REQUIRE(lines[0].time <= lines[1].time);
}

TEST_CASE("a text log counts every line ever pushed, and its revision grows with each change", "[core][text_log]") {
    TextLog log(2);
    const Revision start = log.revision();
    log.push("a");
    log.push("b");
    log.push("c");
    REQUIRE(log.pushed() == 3);
    REQUIRE(log.revision() > start);

    const Revision before = log.revision();
    log.clear();
    REQUIRE(log.size() == 0);
    REQUIRE(log.pushed() == 3); // a reader still learns how many came since
    REQUIRE(log.revision() > before);
}

TEST_CASE("a text log takes lines from several threads at once", "[core][text_log][threads]") {
    TextLog log(1000);
    std::vector<std::thread> writers;
    for (int t = 0; t < 4; ++t) {
        writers.emplace_back([&log, t] {
            for (int i = 0; i < 200; ++i) {
                log.push("thread " + std::to_string(t) + " line " + std::to_string(i));
            }
        });
    }
    std::vector<LogLine> lines;
    for (int i = 0; i < 50; ++i) {
        log.read(lines, 20); // reading meanwhile is safe
    }
    for (std::thread& writer : writers) {
        writer.join();
    }
    REQUIRE(log.pushed() == 800);
    REQUIRE(log.size() == 800);
}
