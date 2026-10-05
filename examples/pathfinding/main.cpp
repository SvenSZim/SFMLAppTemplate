// Pathfinding on a grid, step by step: breadth-first search, Dijkstra and A* find their way from
// a start to a goal around walls and through mud, on a thread of their own, while you watch.
//
//   pathfinding [--smoke-test]
//
// In the grid: the left button paints with the brush (wall, mud or erase), the right button
// erases, and start and goal are dragged to a new place. The middle button moves the view, the
// wheel zooms. Every change starts the search again. Escape quits. --smoke-test draws a few
// frames and exits, for automated checks.
//
// The search runs on the simulation thread and owns the grid; the application sends it what the
// user does as commands, and draws the state it publishes: all cells in one draw call.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"
#include "atpl/app/quad_batch.hpp"
#include "atpl/core/grid.hpp"
#include "atpl/core/random.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/timing.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace atpl;

namespace {

// ----- The world -----

constexpr int columns = 80;
constexpr int rows = 50;

/// What a cell is made of.
enum class Ground : std::uint8_t { Free, Mud, Wall };

/// The cost of entering a cell. Breadth-first search ignores it; the others go round mud when
/// that is cheaper.
float costOf(Ground ground) {
    return ground == Ground::Mud ? 5.f : 1.f;
}

enum class Algorithm { BreadthFirst, Dijkstra, AStar };
constexpr std::array<const char*, 3> algorithmNames{ "Breadth-first", "Dijkstra", "A*" };

enum class Brush { Wall, Mud, Erase };
constexpr std::array<const char*, 3> brushNames{ "Wall", "Mud", "Erase" };

/// What the user can change. The search reads these from its thread.
struct Params {
    Param<Algorithm> algorithm = Algorithm::AStar;
    Param<bool> diagonal = false; ///< Eight neighbours instead of four.
    Param<int> speed = 400;       ///< Cells expanded per second.
    Param<bool> instant = false;  ///< The whole search at once.
    Param<Brush> brush = Brush::Wall;
};

// ----- What the application tells the search -----

struct Paint {
    sf::Vector2i cell;
    Ground ground;
};
struct MoveStart {
    sf::Vector2i cell;
};
struct MoveGoal {
    sf::Vector2i cell;
};
struct Clear {};
struct RandomWalls {
    std::uint64_t seed;
};
struct Maze {
    std::uint64_t seed;
};
struct Restart {};
struct StepOne {};
using Command = std::variant<Paint, MoveStart, MoveGoal, Clear, RandomWalls, Maze, Restart, StepOne>;

// ----- What the main thread draws -----

/// How a cell is shown.
struct CellView {
    Ground ground = Ground::Free;
    bool frontier = false; ///< Waiting to be expanded.
    float visited = -1.f;  ///< When it was expanded, 0 (first) to 1 (latest); below 0: not yet.
};

struct Moment {
    Grid<CellView> cells{ columns, rows };
    sf::Vector2i start;
    sf::Vector2i goal;
    std::vector<sf::Vector2i> path; ///< From start to goal, once found.
    int expanded = 0;
    int frontier = 0;
    float cost = 0.f;
    bool done = false;
    bool found = false;
};

// ----- The search: one cell at a time -----

/// Breadth-first search, Dijkstra and A* in one: they differ only in which waiting cell comes
/// next. `step` expands one cell, so the search can be watched.
class Search {
public:
    void
    restart(const Grid<Ground>& ground, sf::Vector2i start, sf::Vector2i goal, Algorithm algorithm, bool diagonal) {
        m_ground = &ground;
        m_goal = goal;
        m_algorithm = algorithm;
        m_diagonal = diagonal;
        m_cost.resize(ground.width(), ground.height(), unreached);
        m_parent.resize(ground.width(), ground.height(), -1);
        m_order.resize(ground.width(), ground.height(), -1);
        m_waiting.resize(ground.width(), ground.height(), false);
        m_open = {};
        m_fifo.clear();
        m_expanded = 0;
        m_waitingCount = 0;
        m_done = false;
        m_found = false;
        m_cost(start) = 0.f;
        push(m_ground->index(start.x, start.y), 0.f);
    }

    /// Expands the next cell. Returns false once the search is over.
    bool step() {
        if (m_done) {
            return false;
        }
        const std::optional<int> next = pop();
        if (!next) {
            m_done = true; // nothing left to try: there is no way
            return false;
        }
        const sf::Vector2i cell = cellOf(*next);
        m_order(cell) = m_expanded++;
        if (cell == m_goal) {
            m_done = true;
            m_found = true;
            return false;
        }
        const Neighbourhood neighbourhood = m_diagonal ? Neighbourhood::Eight : Neighbourhood::Four;
        m_ground->forEachNeighbour(
            cell.x,
            cell.y,
            [&](int x, int y, const Ground& ground) {
                if (ground == Ground::Wall || m_order(x, y) >= 0) {
                    return;
                }
                const bool slanted = x != cell.x && y != cell.y;
                // No squeezing diagonally between two walls that touch at a corner.
                if (slanted && ((*m_ground)(x, cell.y) == Ground::Wall || (*m_ground)(cell.x, y) == Ground::Wall)) {
                    return;
                }
                const float step = costOf(ground) * (slanted ? std::numbers::sqrt2_v<float> : 1.f);
                const float cost = m_cost(cell) + step;
                // Breadth-first search takes a cell the first time it finds it: the fewest steps.
                // The others take it again whenever they find a cheaper way.
                const bool better =
                    m_algorithm == Algorithm::BreadthFirst ? m_cost(x, y) == unreached : cost < m_cost(x, y);
                if (better) {
                    m_cost(x, y) = cost;
                    m_parent(x, y) = static_cast<int>(m_ground->index(cell.x, cell.y));
                    push(static_cast<int>(m_ground->index(x, y)), cost + heuristic({ x, y }));
                }
            },
            neighbourhood
        );
        return true;
    }

    /// Writes the search as it is now into a moment for drawing.
    void show(Moment& moment) const {
        const float latest = static_cast<float>(std::max(m_expanded - 1, 1));
        for (int y = 0; y < m_ground->height(); ++y) {
            for (int x = 0; x < m_ground->width(); ++x) {
                CellView& view = moment.cells(x, y);
                view.ground = (*m_ground)(x, y);
                view.frontier = m_waiting(x, y) && m_order(x, y) < 0;
                view.visited = m_order(x, y) >= 0 ? static_cast<float>(m_order(x, y)) / latest : -1.f;
            }
        }
        moment.path.clear();
        if (m_found) {
            for (int at = static_cast<int>(m_ground->index(m_goal.x, m_goal.y)); at >= 0; at = m_parent(cellOf(at))) {
                moment.path.push_back(cellOf(at));
            }
            std::reverse(moment.path.begin(), moment.path.end());
        }
        moment.expanded = m_expanded;
        moment.frontier = m_waitingCount;
        moment.cost = m_found ? m_cost(m_goal) : 0.f;
        moment.done = m_done;
        moment.found = m_found;
    }

    [[nodiscard]] int frontier() const { return m_waitingCount; }

private:
    static constexpr float unreached = 1.e9f;

    struct Waiting {
        float priority;
        int order; ///< Among equal priorities, the one that came first.
        int index;
        bool operator>(const Waiting& other) const {
            return priority != other.priority ? priority > other.priority : order > other.order;
        }
    };

    /// A* only: a guess of the rest of the way that is never too high, so the path stays the
    /// cheapest. Every cell costs at least 1.
    [[nodiscard]] float heuristic(sf::Vector2i cell) const {
        if (m_algorithm != Algorithm::AStar) {
            return 0.f;
        }
        const float dx = static_cast<float>(std::abs(cell.x - m_goal.x));
        const float dy = static_cast<float>(std::abs(cell.y - m_goal.y));
        if (!m_diagonal) {
            return dx + dy;
        }
        return std::max(dx, dy) + (std::numbers::sqrt2_v<float> - 1.f) * std::min(dx, dy);
    }

    void push(int index, float priority) {
        const sf::Vector2i cell = cellOf(index);
        if (!m_waiting(cell)) {
            m_waiting(cell) = true;
            ++m_waitingCount;
        }
        if (m_algorithm == Algorithm::BreadthFirst) {
            m_fifo.push_back(index); // first come, first served
        } else {
            m_open.push({ priority, m_pushed++, index });
        }
    }

    std::optional<int> pop() {
        while (true) {
            int index = 0;
            if (m_algorithm == Algorithm::BreadthFirst) {
                if (m_fifo.empty()) {
                    return std::nullopt;
                }
                index = m_fifo.front();
                m_fifo.pop_front();
            } else {
                if (m_open.empty()) {
                    return std::nullopt;
                }
                index = m_open.top().index;
                m_open.pop();
            }
            const sf::Vector2i cell = cellOf(index);
            if (m_order(cell) >= 0) {
                continue; // a cheaper way to it was expanded already
            }
            m_waiting(cell) = false;
            --m_waitingCount;
            return index;
        }
    }

    [[nodiscard]] sf::Vector2i cellOf(int index) const {
        return { index % m_ground->width(), index / m_ground->width() };
    }

    const Grid<Ground>* m_ground = nullptr;
    sf::Vector2i m_goal;
    Algorithm m_algorithm = Algorithm::AStar;
    bool m_diagonal = false;
    Grid<float> m_cost;
    Grid<int> m_parent;
    Grid<int> m_order;
    Grid<bool> m_waiting;
    std::priority_queue<Waiting, std::vector<Waiting>, std::greater<>> m_open;
    std::deque<int> m_fifo;
    int m_pushed = 0;
    int m_expanded = 0;
    int m_waitingCount = 0;
    bool m_done = false;
    bool m_found = false;
};

// ----- The simulation: the grid and the search, on their own thread -----

class Pathfinding final : public Simulation<Moment, Command> {
public:
    explicit Pathfinding(const Params& params) :
        m_params(params) {
        controls.tickRate = 60.0;
        handle(RandomWalls{ 0 }); // something to find a way through from the start
    }

    Series frontierSizes{ 300 }; ///< How many cells wait, as the search goes on.

private:
    void onCommand(const Command& command) override {
        std::visit([this](const auto& c) { handle(c); }, command);
    }

    void handle(const Paint& paint) {
        if (m_ground.contains(paint.cell) && paint.cell != m_start && paint.cell != m_goal) {
            m_ground(paint.cell) = paint.ground;
            restart();
        }
    }
    void handle(const MoveStart& move) { moveEnd(m_start, move.cell); }
    void handle(const MoveGoal& move) { moveEnd(m_goal, move.cell); }
    void handle(const Clear&) {
        m_ground.fill(Ground::Free);
        restart();
    }
    void handle(const RandomWalls& walls) {
        Random random(walls.seed);
        for (Ground& ground : m_ground) {
            const float roll = random.uniform();
            ground = roll < 0.25f ? Ground::Wall : roll < 0.35f ? Ground::Mud : Ground::Free;
        }
        keepEndsFree();
        restart();
    }
    void handle(const Maze& maze) {
        carveMaze(maze.seed);
        keepEndsFree();
        restart();
    }
    void handle(const Restart&) { restart(); }
    void handle(const StepOne&) {
        if (m_search.step()) {
            frontierSizes.push(static_cast<float>(m_search.frontier()));
        }
    }

    void tick(float dt) override {
        // As many cells as the speed says, the fraction carried over to the next tick.
        m_budget += static_cast<double>(m_params.speed.get()) * dt;
        bool expanded = false;
        while (m_params.instant.get() || m_budget >= 1.0) {
            m_budget = std::max(m_budget - 1.0, 0.0);
            if (!m_search.step()) {
                break;
            }
            expanded = true;
            if (m_params.instant.get()) {
                frontierSizes.push(static_cast<float>(m_search.frontier()));
            }
        }
        if (expanded && !m_params.instant.get()) {
            frontierSizes.push(static_cast<float>(m_search.frontier()));
        }
    }

    void writeState(Moment& moment) const override {
        m_search.show(moment);
        moment.start = m_start;
        moment.goal = m_goal;
    }

    void moveEnd(sf::Vector2i& end, sf::Vector2i to) {
        if (m_ground.contains(to) && to != m_start && to != m_goal) {
            end = to;
            m_ground(to) = Ground::Free;
            restart();
        }
    }

    void keepEndsFree() {
        m_ground(m_start) = Ground::Free;
        m_ground(m_goal) = Ground::Free;
    }

    /// A maze by depth-first carving: walls everywhere, then paths cut from cell to cell two
    /// apart, in a random order, until every such cell is reached.
    void carveMaze(std::uint64_t seed) {
        Random random(seed);
        m_ground.fill(Ground::Wall);
        std::vector<sf::Vector2i> stack{ { 1, 1 } };
        m_ground(1, 1) = Ground::Free;
        while (!stack.empty()) {
            const sf::Vector2i cell = stack.back();
            std::array<sf::Vector2i, 4> steps{ { { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 } } };
            std::shuffle(steps.begin(), steps.end(), random);
            bool moved = false;
            for (const sf::Vector2i step : steps) {
                const sf::Vector2i next = cell + step;
                if (next.x > 0 && next.y > 0 && next.x < columns - 1 && next.y < rows - 1 &&
                    m_ground(next) == Ground::Wall) {
                    m_ground(cell + sf::Vector2i(step.x / 2, step.y / 2)) = Ground::Free;
                    m_ground(next) = Ground::Free;
                    stack.push_back(next);
                    moved = true;
                    break;
                }
            }
            if (!moved) {
                stack.pop_back();
            }
        }
        m_start = { 1, 1 };
        m_goal = { columns - 3, rows - 3 }; // both odd: cells the maze reaches
    }

    void restart() {
        m_search.restart(m_ground, m_start, m_goal, m_params.algorithm.get(), m_params.diagonal.get());
        m_budget = 0.0;
        frontierSizes.clear();
    }

    const Params& m_params;
    Grid<Ground> m_ground{ columns, rows, Ground::Free };
    sf::Vector2i m_start{ 10, rows / 2 };
    sf::Vector2i m_goal{ columns - 11, rows / 2 };
    Search m_search;
    double m_budget = 0.0; ///< Cells the speed allows that are not expanded yet.
};

// ----- Drawing: on the main thread, one unit a cell -----

sf::Color mixed(sf::Color a, sf::Color b, float t) {
    const auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return { channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), 255 };
}

/// Draws the grid with one quad per cell, laid out once and recoloured every frame, then the
/// path and the two ends. Colours come from the theme, so the grid follows a theme switch.
class GridPainter {
public:
    GridPainter() {
        m_cells.reserve(static_cast<std::size_t>(columns) * rows);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                m_cells.add(
                    FloatRect(static_cast<float>(x), static_cast<float>(y), 0.94f, 0.94f), sf::Color::Transparent
                );
            }
        }
    }

    void draw(sf::RenderTarget& target, const Moment& moment, const Theme& theme, float pixel) {
        // The ground a little lighter than the panels, so the grid stands out from the window.
        const sf::Color outline = theme.resolve(Panel::Background).border;
        const sf::Color free = mixed(theme.resolve(Panel::Background).color, outline, 0.18f);
        const sf::Color wall = theme.resolve(Paragraph::Footer).color;
        const sf::Color mud = mixed(free, outline, 0.55f);
        const sf::Color accent = theme.resolve(ProgressBar::Fill).color;
        const sf::Color ink = theme.resolve(Paragraph::Heading).color;

        const std::span<const CellView> cells = moment.cells.cells();
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const CellView& cell = cells[i];
            sf::Color color = cell.ground == Ground::Wall ? wall : cell.ground == Ground::Mud ? mud : free;
            if (cell.ground != Ground::Wall) {
                if (cell.frontier) {
                    color = accent;
                } else if (cell.visited >= 0.f) {
                    color = mixed(color, accent, 0.2f + 0.35f * cell.visited); // the latest the brightest
                }
            }
            m_cells.setColor(i, color);
        }
        target.draw(m_cells);

        // The path through the cell centres, a little thicker than a pixel at any zoom.
        if (moment.path.size() > 1) {
            QuadBatch line;
            const float width = std::max(0.18f, 2.f * pixel);
            for (std::size_t i = 1; i < moment.path.size(); ++i) {
                const sf::Vector2f a = sf::Vector2f(moment.path[i - 1]) + sf::Vector2f(0.47f, 0.47f);
                const sf::Vector2f b = sf::Vector2f(moment.path[i]) + sf::Vector2f(0.47f, 0.47f);
                const sf::Vector2f along = b - a;
                line.add((a + b) * 0.5f, { along.length() + width, width }, along.angle(), ink);
            }
            target.draw(line);
        }

        // The ends: the start filled, the goal a ring.
        sf::CircleShape end(0.38f, 24);
        end.setOrigin({ 0.38f, 0.38f });
        end.setPosition(sf::Vector2f(moment.start) + sf::Vector2f(0.47f, 0.47f));
        end.setFillColor(ink);
        target.draw(end);
        end.setPosition(sf::Vector2f(moment.goal) + sf::Vector2f(0.47f, 0.47f));
        end.setFillColor(sf::Color::Transparent);
        end.setOutlineColor(ink);
        end.setOutlineThickness(std::max(0.12f, 2.f * pixel));
        target.draw(end);
    }

private:
    QuadBatch m_cells;
};

// ----- Editing: forwarded input over the grid -----

/// Turns presses and drags in the grid view into commands: painting, erasing, moving the ends.
class Editor {
public:
    Editor(Pathfinding& simulation, const Params& params, const Camera& camera) :
        m_simulation(simulation),
        m_params(params),
        m_camera(camera) {}

    /// Returns true if the event was an edit.
    bool handle(const Event& event, const Moment& moment) {
        if (const auto* press = event.getIf<PointerPressed>(); press != nullptr && press->pointer.isIn("grid")) {
            const sf::Vector2i cell = cellAt(press->pointer.inView);
            if (press->button == sf::Mouse::Button::Left && cell == moment.start) {
                m_dragging = Dragging::Start;
            } else if (press->button == sf::Mouse::Button::Left && cell == moment.goal) {
                m_dragging = Dragging::Goal;
            } else if (press->button == sf::Mouse::Button::Left || press->button == sf::Mouse::Button::Right) {
                m_dragging = Dragging::Paint;
                m_ground = press->button == sf::Mouse::Button::Right ? Ground::Free : groundOf(m_params.brush.get());
                paint(cell, cell);
            }
            m_last = cell;
            return m_dragging != Dragging::None;
        }
        if (const auto* move = event.getIf<PointerMoved>(); move != nullptr && m_dragging != Dragging::None) {
            const sf::Vector2i cell = cellAt(move->pointer.inView);
            if (cell != m_last) {
                if (m_dragging == Dragging::Paint) {
                    paint(m_last, cell);
                } else {
                    m_simulation.send(
                        m_dragging == Dragging::Start ? Command(MoveStart{ cell }) : Command(MoveGoal{ cell })
                    );
                }
                m_last = cell;
            }
            return true;
        }
        if (event.is<PointerReleased>()) {
            m_dragging = Dragging::None;
        }
        return false;
    }

private:
    enum class Dragging { None, Paint, Start, Goal };

    static Ground groundOf(Brush brush) {
        return brush == Brush::Wall ? Ground::Wall : brush == Brush::Mud ? Ground::Mud : Ground::Free;
    }

    [[nodiscard]] sf::Vector2i cellAt(sf::Vector2f inView) const {
        const sf::Vector2f world = m_camera.toWorld(inView);
        return { static_cast<int>(std::floor(world.x)), static_cast<int>(std::floor(world.y)) };
    }

    /// Every cell on the line from `from` to `to`, so a fast drag leaves no gaps.
    void paint(sf::Vector2i from, sf::Vector2i to) {
        const int steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
        for (int i = 0; i <= steps; ++i) {
            const float t = steps == 0 ? 0.f : static_cast<float>(i) / static_cast<float>(steps);
            const sf::Vector2i cell(
                static_cast<int>(std::lround(static_cast<float>(from.x) + static_cast<float>(to.x - from.x) * t)),
                static_cast<int>(std::lround(static_cast<float>(from.y) + static_cast<float>(to.y - from.y) * t))
            );
            m_simulation.send(Paint{ cell, m_ground });
        }
    }

    Pathfinding& m_simulation;
    const Params& m_params;
    const Camera& m_camera;
    Dragging m_dragging = Dragging::None;
    Ground m_ground = Ground::Wall;
    sf::Vector2i m_last;
};

// ----- The application -----

/// What the main thread reports in the statistics panel.
struct Report {
    Param<double> expanded;
    Param<double> frontier;
    Param<std::string> result;
};

int run(bool smokeTest) {
    Params params;
    Pathfinding simulation(params);
    Report report;

    // The middle button moves the view, so the left one can paint.
    Camera camera("grid", { .dragButton = sf::Mouse::Button::Middle });
    // The whole grid, with room at the sides for the panels.
    camera.show({ -columns * 0.4f, -1.f }, { columns * 1.8f, rows + 2.f });

    App app({
        .window = {.title = "atpl pathfinding"},
        .ui = {
            .theme = themes::colorful(),
            .background = "grid",
            .defaultView = "grid",
            .panels = {
                // One panel in each corner, so that they leave the middle of the grid free.
                {
                    .name = "Search",
                    .placement = Anchor::TopLeft,
                    .columns = 2,
                    .widgets = {
                        at({.column = 0, .row = 0, .columnSpan = 2},
                           Dropdown("Algorithm", {algorithmNames.begin(), algorithmNames.end()}, params.algorithm)),
                        at({.column = 0, .row = 1, .columnSpan = 2},
                           Slider("Speed", params.speed, {.min = 10.0, .max = 2000.0, .step = 10.0, .format = "{:.0f} /s"})),
                        at({.column = 0, .row = 2}, Switch("Diagonal", params.diagonal)),
                        at({.column = 1, .row = 2}, Switch("Instant", params.instant)),
                        at({.column = 0, .row = 3}, Switch("Paused", simulation.controls.paused)),
                        at({.column = 1, .row = 3}, Button("Step")),
                        at({.column = 0, .row = 4, .columnSpan = 2}, Button("Restart")),
                    },
                },
                {
                    .name = "Grid",
                    .placement = Anchor::BottomLeft,
                    .columns = 2,
                    .widgets = {
                        at({.column = 0, .row = 0, .columnSpan = 2},
                           Dropdown("Brush", {brushNames.begin(), brushNames.end()}, params.brush)),
                        at({.column = 0, .row = 1}, Button("Random walls")),
                        at({.column = 1, .row = 1}, Button("Maze")),
                        at({.column = 0, .row = 2, .columnSpan = 2}, Button("Clear")),
                    },
                },
                {
                    .name = "Statistics",
                    .placement = Anchor::TopRight,
                    .widgets = {
                        TextDisplay("Result", report.result),
                        ValueDisplay("Expanded", report.expanded, {.format = "{:.0f}"}),
                        ValueDisplay("Waiting", report.frontier, {.format = "{:.0f}"}),
                        Graph("Waiting cells", simulation.frontierSizes),
                    },
                },
                {
                    .name = "Help",
                    .placement = Anchor::BottomRight,
                    .widgets = {
                        Paragraph("How", {.text = "Left button: paint with the brush. Right button: erase. Drag the "
                                                  "start or the goal to move it. Middle button: move the view; "
                                                  "wheel: zoom.",
                                          .footer = "Escape quits."}),
                    },
                },
            },
        },
        .scale = AutoScale{},
    });

    GridPainter painter;
    Editor editor(simulation, params, camera);
    std::uint64_t seed = 1; // the next layout (0 was the first); the same seeds give the same walls

    app.ui().view("grid").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        painter.draw(target, simulation.state(), app.ui().theme(), app.ui().scale() / camera.zoom());
    });

    app.onEvent([&](const Event& event) {
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }
        if (event.isButton("Step")) {
            simulation.controls.paused = true;
            simulation.send(StepOne{});
        }
        if (event.isButton("Restart")) {
            simulation.send(Restart{});
        }
        if (event.isButton("Clear")) {
            simulation.send(Clear{});
        }
        if (event.isButton("Random walls")) {
            simulation.send(RandomWalls{ seed++ });
        }
        if (event.isButton("Maze")) {
            simulation.send(Maze{ seed++ });
        }
        // A different search: start it again.
        if (const ValueChanged* changed = event.changeOf("Algorithm"); changed != nullptr && changed->final) {
            simulation.send(Restart{});
        }
        if (const ValueChanged* changed = event.changeOf("Diagonal"); changed != nullptr && changed->final) {
            simulation.send(Restart{});
        }

        const bool edited = editor.handle(event, simulation.state());
        const bool moved = camera.handle(event);
        if (edited || moved) {
            app.ui().requestRedraw();
        }
    });

    int passesLeft = 5; // only counted in a smoke test
    app.onUpdate([&](float) {
        const Moment& moment = simulation.state();
        report.expanded = moment.expanded;
        report.frontier = moment.frontier;
        report.result = !moment.done   ? std::string("Searching ...")
                        : moment.found ? std::format("Path of {} cells, cost {:.1f}", moment.path.size(), moment.cost)
                                       : std::string("No way to the goal");
        if (smokeTest) {
            app.ui().requestRedraw();
            if (--passesLeft == 0) {
                app.quit();
            }
        }
    });

    return app.run(simulation);
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        return run(argc > 1 && std::string_view(argv[1]) == "--smoke-test");
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
