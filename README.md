# SFMLAppTemplate

A template for SFML applications that show a simulation or an algorithm at work: AI training, pathfinding, raytracing, particle systems.

It provides a light, polished UI that is described in a few lines, linked to the application's data without glue code, and never slows the simulation down, plus the utilities such applications usually need.

> **Status: rework in progress.** The structure and all design decisions are settled. The repository currently contains the skeleton of the new structure (Phase 0) and the first utilities; the UI itself is not implemented yet. See the [project board](https://github.com/users/SvenSZim/projects/3) for progress.

## Goals

- **Easy to set up**: a new application is a few dozen lines of declarative setup.
- **Light-weight**: the UI uses no more CPU, GPU or memory than necessary, and none while idle.
- **Polished**: rounded cards, shadows, animations, consistent themes.
- **Linked to your data**: a slider changes your parameter directly, safely across threads.
- **Built for simulations**: your rendering goes into views (background, panels, minimap); your simulation runs on its own thread.
- **Utilities included**: thread pool, random numbers, timing, grid and more.

What an application will look like is sketched in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#7-what-the-application-writes).

## Building

Requirements:
- CMake 3.28 or newer
- A C++20 compiler (GCC is the tested one; Linux is the supported platform)
- On Linux, the development packages SFML needs for its graphics module:

  ```
  sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
                   libudev-dev libgl1-mesa-dev libfreetype-dev
  ```

SFML and Catch2 are downloaded by CMake; nothing else has to be installed.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Binaries are placed in `build/bin/`. `build/bin/minimal` is the smallest example: a window with one line of text that closes with Escape.

The build copies `resources/` to `build/bin/resources/`. Applications look for their resources next to the executable, not in the working directory, so they can be started from anywhere.

For editors and language servers, CMake writes `compile_commands.json` into the build directory and links it into the repository root (not on Windows).

| CMake option | Default | Effect |
|---|---|---|
| `ATPL_BUILD_TESTS` | `ON` | Build the unit tests |
| `ATPL_BUILD_EXAMPLES` | `ON` | Build the example applications |
| `ATPL_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |
| `ATPL_LINK_COMPILE_COMMANDS` | `ON` | Link `compile_commands.json` into the repository root |

## Libraries

| Target | Content | Depends on |
|---|---|---|
| `atpl::core` | Threading, parameters, timing, utilities | C++ standard library only |
| `atpl::ui` | Panels, widgets, layout, input, theme, rendering | `atpl::core`, SFML Graphics |
| `atpl::app` | Window, main loop, simulation thread | `atpl::ui` |

## Documentation

| Document | Content |
|---|---|
| [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md) | Vision, requirements, decision log, roadmap |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Target structure, responsibilities, frame flow |
| [docs/CODE_STYLE.md](docs/CODE_STYLE.md) | Code conventions |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Workflow: issues, branches, commits, pull requests |

## License

MIT, see [LICENSE](LICENSE). Bundled third-party material is listed in [THIRD_PARTY.md](THIRD_PARTY.md).
