#include "atpl/app/resources.hpp"

#include "app/executable_path.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

using atpl::ResourceError;
using atpl::Resources;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;
namespace fs = std::filesystem;

namespace {

/// A directory that exists for the duration of one test.
class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string& name) :
        m_path(fs::temp_directory_path() / ("atpl_test_" + name)) {
        fs::remove_all(m_path);
        fs::create_directories(m_path);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(m_path, ignored);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const fs::path& path() const { return m_path; }

    void write(const fs::path& relative, const std::string& content) const {
        fs::create_directories((m_path / relative).parent_path());
        std::ofstream(m_path / relative) << content;
    }

private:
    fs::path m_path;
};

} // namespace

TEST_CASE("the executable directory is the one that holds the running binary", "[app][resources]") {
    const fs::path directory = atpl::executableDirectory();

    REQUIRE(directory.is_absolute());
    REQUIRE(fs::is_directory(directory));
#ifdef __linux__
    REQUIRE(fs::exists(directory / "atpl_app_tests"));
#endif
}

TEST_CASE("argv0 gives the executable directory when it carries a path", "[app][resources]") {
    using atpl::app::executableDirectoryFromArgv0;
    const fs::path cwd = "/home/user/project";

    REQUIRE(executableDirectoryFromArgv0("/opt/app/bin/demo", cwd) == fs::path("/opt/app/bin"));
    REQUIRE(executableDirectoryFromArgv0("./demo", cwd) == fs::path("/home/user/project"));
    REQUIRE(executableDirectoryFromArgv0("build/bin/demo", cwd) == fs::path("/home/user/project/build/bin"));
    REQUIRE(executableDirectoryFromArgv0("../bin/demo", cwd) == fs::path("/home/user/bin"));
}

TEST_CASE("argv0 without a directory gives nothing", "[app][resources]") {
    using atpl::app::executableDirectoryFromArgv0;

    // Started through PATH: argv0 says nothing about where the program is.
    REQUIRE_FALSE(executableDirectoryFromArgv0("demo", "/home/user").has_value());
    REQUIRE_FALSE(executableDirectoryFromArgv0("", "/home/user").has_value());
}

TEST_CASE("a resource path is resolved inside the resource directory", "[app][resources]") {
    const TemporaryDirectory directory("resolve");
    directory.write("fonts/some.ttf", "x");
    const Resources resources(directory.path());

    REQUIRE(resources.root() == directory.path());
    REQUIRE(resources.path("fonts/some.ttf") == directory.path() / "fonts" / "some.ttf");
}

TEST_CASE("a missing resource fails loudly and names the full path", "[app][resources]") {
    const TemporaryDirectory directory("missing");
    const Resources resources(directory.path());
    const std::string expected = (directory.path() / "fonts" / "nope.ttf").string();

    REQUIRE_THROWS_MATCHES(
        resources.path("fonts/nope.ttf"),
        ResourceError,
        MessageMatches(ContainsSubstring("Resource not found") && ContainsSubstring(expected))
    );
    REQUIRE_THROWS_AS(resources.loadFont("fonts/nope.ttf"), ResourceError);
}

TEST_CASE("a directory is not accepted as a resource", "[app][resources]") {
    const TemporaryDirectory directory("isdir");
    directory.write("fonts/some.ttf", "x");
    const Resources resources(directory.path());

    REQUIRE_THROWS_AS(resources.path("fonts"), ResourceError);
}

TEST_CASE("a missing resource directory is reported as such", "[app][resources]") {
    const Resources resources(fs::temp_directory_path() / "atpl_test_does_not_exist");

    REQUIRE_THROWS_MATCHES(
        resources.path("fonts/default.ttf"),
        ResourceError,
        MessageMatches(ContainsSubstring("Resource directory not found"))
    );
}

TEST_CASE("a file that is not a font fails loudly", "[app][resources]") {
    const TemporaryDirectory directory("notfont");
    directory.write("fonts/broken.ttf", "this is not a font");
    const Resources resources(directory.path());

    REQUIRE_THROWS_MATCHES(
        resources.loadFont("fonts/broken.ttf"), ResourceError, MessageMatches(ContainsSubstring("Not a usable font"))
    );
}

TEST_CASE("the bundled font is found next to the executable and loads", "[app][resources]") {
    const Resources resources = Resources::nextToExecutable();

    const sf::Font font = resources.loadFont("fonts/default.ttf");

    REQUIRE(font.getInfo().family == "Inconsolata");
}
