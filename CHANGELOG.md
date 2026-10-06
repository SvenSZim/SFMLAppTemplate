# Changelog

Versions follow [semantic versioning](https://semver.org/): until 1.0, a minor version may change the API.

## 0.1.0 (2026-10-06)

The first release: a template for SFML 3 applications that show a simulation or an algorithm at work.

- **UI** (`atpl::ui`): panels placed by anchors or in the window's grid, card stacks, scrolling; twelve widgets (button, switch, slider, dropdown, text input, value display, progress bar, graph, log, text display, paragraph, view) bound to thread-safe values without glue code; widgets of an application's own on the same interface. Themes (`moon`, `colorful`) and layout themes (`overlay`, `dashboard`, `cards`, `compact`), switched at runtime; part entries for single parts; eased hover, press and toggle animations; shadows and fading outlines; a GUI scale. A frame is drawn only when something changed; an idle application uses no processor time.
- **Application** (`atpl::app`): the window and main loop; a simulation on its own thread at a fixed step with pause, steps and speed; views drawn by the application, with a camera and a minimap; resources by name; a quad batch for many sprites in one draw call.
- **Utilities** (`atpl::core`): a thread pool with `parallelFor`, random numbers that are the same for a seed on every platform, timing helpers, a 2D grid.
- **Examples**: `starter`, `showcase`, `pathfinding`, `particles`; `new-project/` to start an application of your own.
- Linux is supported and tested with GCC and Clang, also under sanitizers; Windows (MSVC) is checked.
