// Particles: tens of thousands of them in a box, pushing each other apart, pulled by gravity and
// by attractors you place, computed on all cores on a thread of their own.
//
//   particles [--smoke-test]
//
// In the main view, the Tool decides what the left button does: pull particles towards the
// pointer, place a spawner, an attractor or a repulsor (drag one to move it), or erase one. The
// right button erases too, the middle button moves the view, the wheel zooms. Escape quits.
// --smoke-test draws a few frames and exits, for automated checks.
//
// Every tick sorts the particles into buckets the size of their reach, so that each one only
// looks at its neighbours, and then spreads the forces and the movement over the cores with
// `parallelFor`. The main thread draws the newest state: all particles in one draw call.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"
#include "atpl/app/minimap.hpp"
#include "atpl/app/quad_batch.hpp"
#include "atpl/core/grid.hpp"
#include "atpl/core/random.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/thread_pool.hpp"
#include "atpl/core/timing.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

using namespace atpl;

namespace {

// ----- The world -----

/// The box, in world units.
constexpr sf::Vector2f boxSize(1600.f, 900.f);

/// How far a particle pushes others: also the size of a bucket.
constexpr float reach = 4.f;
constexpr int bucketColumns = static_cast<int>(boxSize.x / reach);
constexpr int bucketRows = static_cast<int>(boxSize.y / reach);

/// The density map's cells, in world units.
constexpr float densityCell = 10.f;
constexpr int densityColumns = static_cast<int>(boxSize.x / densityCell);
constexpr int densityRows = static_cast<int>(boxSize.y / densityCell);

/// How far an attractor or a repulsor reaches, and the pull of the Pull tool.
constexpr float sourceReach = 220.f;
constexpr float pullReach = 160.f;

enum class Spawn { Uniform, Ring, Clusters, Spiral, Custom };
constexpr std::array<const char*, 5> layoutNames{ "Uniform", "Ring", "Two clusters", "Spiral", "Custom" };

enum class Gravity { None, Centre, Down };
constexpr std::array<const char*, 3> gravityNames{ "None", "Centre", "Down" };

enum class Render { Particles, Speed, Density };
constexpr std::array<const char*, 3> renderNames{ "Plain",
                                                  "Speed",
                                                  "Density" }; // particles, coloured by speed, or a map

enum class Tool { Pull, Spawner, Attractor, Repulsor, Erase };
constexpr std::array<const char*, 5> toolNames{ "Pull", "Spawner", "Attractor", "Repulsor", "Erase" };

/// What the user can change. The simulation reads these from its thread.
struct Params {
    Param<Spawn> layout = Spawn::Ring;
    Param<int> count = 40000;
    Param<Gravity> gravity = Gravity::Centre;
    Param<float> gravityStrength = 60.f;
    Param<float> push = 300.f;     ///< How hard particles push each other apart.
    Param<float> friction = 0.05f; ///< Share of the speed lost per second.
    Param<Tool> tool = Tool::Pull;
    Param<Render> render = Render::Speed;

    // The Pull tool, while the left button is held in the view.
    Param<bool> pulling = false;
    Param<float> pullX = 0.f;
    Param<float> pullY = 0.f;
};

/// Something placed in the box.
struct Marker {
    enum class Kind { Spawner, Attractor, Repulsor };
    Kind kind;
    sf::Vector2f position;
};

// ----- What the application tells the simulation -----

struct Place {
    Marker marker;
};
struct Move {
    std::size_t index;
    sf::Vector2f position;
};
struct Remove {
    std::size_t index;
};
struct ClearMarkers {};
struct Respawn {};
using Command = std::variant<Place, Move, Remove, ClearMarkers, Respawn>;

// ----- What the main thread draws -----

struct Moment {
    std::vector<sf::Vector2f> positions;
    std::vector<float> speeds;
    Grid<float> density{ densityColumns, densityRows }; ///< Particles per density cell; empty unless shown.
    std::vector<Marker> markers;
    std::uint64_t tick = 0; ///< Which tick this is: the drawing is rebuilt only for a new one.
};

// ----- The simulation -----

class Particles final : public Simulation<Moment, Command> {
public:
    explicit Particles(const Params& params) :
        m_params(params) {
        controls.tickRate = 60.0;
        spawn();
    }

    // What it reports, for widgets to show. Thread-safe: it writes them, the UI reads them.
    Series energy{ 300 };        ///< Kinetic energy per particle.
    Param<double> particleCount; ///< How many there are.
    Param<double> parallelShare; ///< Percent of a tick spent in parallelFor.

private:
    void onCommand(const Command& command) override {
        std::visit([this](const auto& c) { handle(c); }, command);
    }
    void handle(const Place& place) { m_markers.push_back(place.marker); }
    void handle(const Move& move) {
        if (move.index < m_markers.size()) {
            m_markers[move.index].position = clampToBox(move.position);
        }
    }
    void handle(const Remove& remove) {
        if (remove.index < m_markers.size()) {
            m_markers.erase(m_markers.begin() + static_cast<std::ptrdiff_t>(remove.index));
        }
    }
    void handle(const ClearMarkers&) { m_markers.clear(); }
    void handle(const Respawn&) { spawn(); }

    void tick(float dt) override {
        const Stopwatch whole;
        sortIntoBuckets();

        // What every particle feels this tick, read once.
        const Forces forces = currentForces();

        // Forces: each part of the loop changes only its own particles' velocities and reads
        // the positions, which no one changes until the next loop.
        const Stopwatch parallel;
        const std::size_t count = m_positions.size();
        m_pool.parallelFor(
            count,
            [&](std::size_t start, std::size_t end) {
                for (std::size_t k = start; k < end; ++k) {
                    const std::size_t i = m_order[k]; // in bucket order: neighbours lie close in memory
                    m_velocities[i] += accelerationOf(i, forces) * dt;
                    m_velocities[i] *= std::max(0.f, 1.f - forces.friction * dt);
                }
            },
            256
        );

        // Movement, and the walls. Per part: the kinetic energy, added up afterwards.
        std::vector<double>& sums = m_energyParts;
        std::fill(sums.begin(), sums.end(), 0.0);
        m_pool.parallelFor(
            count,
            [&](std::size_t start, std::size_t end, std::size_t part) {
                for (std::size_t i = start; i < end; ++i) {
                    m_positions[i] += m_velocities[i] * dt;
                    bounce(m_positions[i], m_velocities[i]);
                    sums[part] += 0.5 * static_cast<double>(m_velocities[i].lengthSquared());
                }
            },
            1024
        );
        const double parallelSeconds = parallel.seconds();

        double total = 0.0;
        for (const double sum : sums) {
            total += sum;
        }
        energy.push(count > 0 ? static_cast<float>(total / static_cast<double>(count)) : 0.f);
        m_shareAverage.add(100.0 * parallelSeconds / std::max(whole.seconds(), 1e-9));
        parallelShare = m_shareAverage.average();
        ++m_tick;
    }

    void writeState(Moment& moment) const override {
        moment.positions = m_positions; // the vectors keep their memory between states
        moment.speeds.resize(m_velocities.size());
        for (std::size_t i = 0; i < m_velocities.size(); ++i) {
            moment.speeds[i] = m_velocities[i].length();
        }
        moment.markers = m_markers;
        moment.tick = m_tick;
        if (m_params.render.get() == Render::Density) {
            moment.density.fill(0.f);
            for (const sf::Vector2f p : m_positions) {
                const int x = std::clamp(static_cast<int>(p.x / densityCell), 0, densityColumns - 1);
                const int y = std::clamp(static_cast<int>(p.y / densityCell), 0, densityRows - 1);
                moment.density(x, y) += 1.f;
            }
        }
    }

    // ----- Spawning -----

    /// Places every particle anew, as the layout says. The same layout always starts the same.
    void spawn() {
        const auto count = static_cast<std::size_t>(std::clamp(m_params.count.get(), 1000, 100000));
        m_positions.resize(count);
        m_velocities.assign(count, { 0.f, 0.f });
        particleCount = static_cast<double>(count);

        std::vector<sf::Vector2f> spawners;
        for (const Marker& marker : m_markers) {
            if (marker.kind == Marker::Kind::Spawner) {
                spawners.push_back(marker.position);
            }
        }
        const Spawn layout =
            m_params.layout.get() == Spawn::Custom && spawners.empty() ? Spawn::Uniform : m_params.layout.get();
        const sf::Vector2f centre = boxSize * 0.5f;

        // One random stream per part of the loop: the same numbers whichever thread runs it.
        RandomStreams streams(static_cast<std::uint64_t>(layout) + 1, m_pool.maxParts());
        m_pool.parallelFor(count, [&](std::size_t start, std::size_t end, std::size_t part) {
            Random& random = streams[part];
            for (std::size_t i = start; i < end; ++i) {
                sf::Vector2f& p = m_positions[i];
                switch (layout) {
                    case Spawn::Uniform:
                        p = { random.range(0.f, boxSize.x), random.range(0.f, boxSize.y) };
                        break;
                    case Spawn::Ring: {
                        const float radius = random.normal(300.f, 25.f);
                        const float angle = random.angle();
                        p = centre + sf::Vector2f(std::cos(angle), std::sin(angle)) * radius;
                        // Moving round the centre as fast as an orbit under gravity to the
                        // centre: the pull, g, is the same at every distance, so v = sqrt(g r).
                        const float orbit = std::sqrt(std::max(m_params.gravityStrength.get(), 0.f) * radius);
                        m_velocities[i] = sf::Vector2f(-std::sin(angle), std::cos(angle)) * orbit;
                        break;
                    }
                    case Spawn::Clusters: {
                        const sf::Vector2f at = i % 2 == 0 ? sf::Vector2f(boxSize.x * 0.3f, centre.y)
                                                           : sf::Vector2f(boxSize.x * 0.7f, centre.y);
                        p = at + random.inDisc<sf::Vector2f>(160.f);
                        break;
                    }
                    case Spawn::Spiral: {
                        const float along = random.uniform();
                        const float arm = static_cast<float>(i % 3) * 2.f * std::numbers::pi_v<float> / 3.f;
                        const float angle = arm + along * 3.f * std::numbers::pi_v<float>;
                        const float radius = 30.f + along * 380.f;
                        p = centre + sf::Vector2f(std::cos(angle), std::sin(angle)) * radius +
                            random.inDisc<sf::Vector2f>(18.f);
                        break;
                    }
                    case Spawn::Custom:
                        p = spawners[i % spawners.size()] + random.inDisc<sf::Vector2f>(70.f);
                        break;
                }
                p = clampToBox(p);
            }
        });
        m_order.resize(count);
    }

    // ----- One tick -----

    /// Sorts the particles into buckets of the size of their reach: a counting sort, so that a
    /// particle's neighbours are found in the 3 by 3 buckets around its own.
    void sortIntoBuckets() {
        const std::size_t count = m_positions.size();
        m_bucketOf.resize(count);
        m_pool.parallelFor(
            count,
            [&](std::size_t start, std::size_t end) {
                for (std::size_t i = start; i < end; ++i) {
                    const sf::Vector2f p = m_positions[i];
                    const int x = std::clamp(static_cast<int>(p.x / reach), 0, bucketColumns - 1);
                    const int y = std::clamp(static_cast<int>(p.y / reach), 0, bucketRows - 1);
                    m_bucketOf[i] = static_cast<int>(m_bucketStart.index(x, y));
                }
            },
            4096
        );

        // Counts, then where each bucket starts, then the particles in bucket order.
        std::span<int> starts = m_bucketStart.cells();
        std::fill(starts.begin(), starts.end(), 0);
        for (const int bucket : m_bucketOf) {
            ++starts[static_cast<std::size_t>(bucket)];
        }
        int sum = 0;
        for (int& start : starts) {
            const int here = start;
            start = sum;
            sum += here;
        }
        m_bucketEnd.assign(starts.begin(), starts.end());
        for (std::size_t i = 0; i < count; ++i) {
            m_order[static_cast<std::size_t>(m_bucketEnd[static_cast<std::size_t>(m_bucketOf[i])]++)] =
                static_cast<std::uint32_t>(i);
        }
    }

    struct Source {
        sf::Vector2f position;
        float strength; ///< Positive pulls, negative pushes.
        float reach;
    };

    struct Forces {
        Gravity gravity;
        float gravityStrength;
        float push;
        float friction;
        std::vector<Source> sources;
    };

    [[nodiscard]] Forces currentForces() const {
        Forces forces{
            m_params.gravity.get(), m_params.gravityStrength.get(), m_params.push.get(), m_params.friction.get(), {}
        };
        for (const Marker& marker : m_markers) {
            if (marker.kind == Marker::Kind::Attractor) {
                forces.sources.push_back({ marker.position, 400.f, sourceReach });
            } else if (marker.kind == Marker::Kind::Repulsor) {
                forces.sources.push_back({ marker.position, -600.f, sourceReach });
            }
        }
        if (m_params.pulling.get()) {
            forces.sources.push_back({ { m_params.pullX.get(), m_params.pullY.get() }, 900.f, pullReach });
        }
        return forces;
    }

    /// What particle `i` feels: its neighbours pushing, gravity, and the sources nearby.
    [[nodiscard]] sf::Vector2f accelerationOf(std::size_t i, const Forces& forces) const {
        const sf::Vector2f p = m_positions[i];
        sf::Vector2f a;

        // Neighbours: the closer, the harder they push.
        const int bucket = m_bucketOf[i];
        const int bx = bucket % bucketColumns;
        const int by = bucket / bucketColumns;
        for (int y = std::max(by - 1, 0); y <= std::min(by + 1, bucketRows - 1); ++y) {
            for (int x = std::max(bx - 1, 0); x <= std::min(bx + 1, bucketColumns - 1); ++x) {
                const std::size_t b = m_bucketStart.index(x, y);
                const int from = m_bucketStart.cells()[b];
                const int to = m_bucketEnd[b];
                for (int k = from; k < to; ++k) {
                    const std::size_t j = m_order[static_cast<std::size_t>(k)];
                    if (j == i) {
                        continue;
                    }
                    const sf::Vector2f d = p - m_positions[j];
                    const float distance2 = d.lengthSquared();
                    if (distance2 < reach * reach && distance2 > 1e-8f) {
                        const float distance = std::sqrt(distance2);
                        a += d / distance * (forces.push * (1.f - distance / reach));
                    }
                }
            }
        }

        // Gravity.
        if (forces.gravity == Gravity::Down) {
            a.y += forces.gravityStrength;
        } else if (forces.gravity == Gravity::Centre) {
            const sf::Vector2f toCentre = boxSize * 0.5f - p;
            const float distance = std::max(toCentre.length(), 20.f); // softened near the centre
            a += toCentre / distance * forces.gravityStrength;
        }

        // Attractors, repulsors and the Pull tool: strongest close by, nothing at their reach.
        for (const Source& source : forces.sources) {
            const sf::Vector2f d = source.position - p;
            const float distance = d.length();
            if (distance < source.reach && distance > 1.f) {
                a += d / distance * (source.strength * (1.f - distance / source.reach));
            }
        }
        return a;
    }

    /// Keeps a particle in the box: off the wall, half as fast and the other way.
    static void bounce(sf::Vector2f& p, sf::Vector2f& v) {
        if (p.x < 0.f || p.x > boxSize.x) {
            p.x = std::clamp(p.x, 0.f, boxSize.x);
            v.x *= -0.5f;
        }
        if (p.y < 0.f || p.y > boxSize.y) {
            p.y = std::clamp(p.y, 0.f, boxSize.y);
            v.y *= -0.5f;
        }
    }

    static sf::Vector2f clampToBox(sf::Vector2f p) {
        return { std::clamp(p.x, 0.f, boxSize.x), std::clamp(p.y, 0.f, boxSize.y) };
    }

    const Params& m_params;
    ThreadPool m_pool; ///< One core left for the UI's thread.
    std::vector<sf::Vector2f> m_positions;
    std::vector<sf::Vector2f> m_velocities;
    std::vector<Marker> m_markers;

    Grid<int> m_bucketStart{ bucketColumns, bucketRows }; ///< Where each bucket's particles start in m_order.
    std::vector<int> m_bucketEnd;                         ///< And where they end.
    std::vector<int> m_bucketOf;                          ///< Each particle's bucket.
    std::vector<std::uint32_t> m_order;                   ///< The particles, bucket after bucket.

    std::vector<double> m_energyParts = std::vector<double>(m_pool.maxParts(), 0.0);
    RunningAverage m_shareAverage{ 60 };
    std::uint64_t m_tick = 0;
};

// ----- Drawing: on the main thread, in world units -----

sf::Color mixed(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    const auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return { channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a) };
}

/// Draws a moment. The particles are one quad batch, rebuilt only for a new tick, a new render
/// mode or a new zoom, and shared by the main view and the minimap.
class WorldPainter {
public:
    explicit WorldPainter(const sf::Texture& dot) :
        m_particles(&dot) {
        m_density.reserve(static_cast<std::size_t>(densityColumns) * densityRows);
        for (int y = 0; y < densityRows; ++y) {
            for (int x = 0; x < densityColumns; ++x) {
                m_density.add(
                    FloatRect(
                        static_cast<float>(x) * densityCell,
                        static_cast<float>(y) * densityCell,
                        densityCell,
                        densityCell
                    ),
                    sf::Color::Transparent
                );
            }
        }
    }

    /// `pixel` is how many world units a pixel is in the main view.
    void draw(sf::RenderTarget& target, const Moment& moment, Render render, const Theme& theme, float pixel) {
        const sf::Color accent = theme.resolve(ProgressBar::Fill).color;
        const sf::Color ink = theme.resolve(Paragraph::Heading).color;
        const sf::Color slow = theme.resolve(Panel::Background).border;

        sf::RectangleShape box(boxSize);
        box.setFillColor(
            mixed(theme.resolve(Panel::Background).color, slow, 0.12f)
        ); // a little lighter than the panels
        box.setOutlineColor(slow);
        box.setOutlineThickness(1.5f * pixel);
        target.draw(box);

        if (render == Render::Density) {
            const std::span<const float> cells = moment.density.cells();
            const float average = static_cast<float>(moment.positions.size()) / static_cast<float>(cells.size());
            for (std::size_t i = 0; i < cells.size(); ++i) {
                sf::Color color =
                    cells[i] <= 0.f ? sf::Color::Transparent : mixed(accent, ink, cells[i] / (average * 6.f) - 0.3f);
                color.a = static_cast<std::uint8_t>(std::min(cells[i] / (average * 2.f), 1.f) * 255.f);
                m_density.setColor(i, color);
            }
            target.draw(m_density);
        } else {
            rebuildParticles(moment, render, accent, ink, slow, pixel);
            target.draw(m_particles);
        }
        drawMarkers(target, moment, accent, ink, pixel);
    }

    /// The minimap: the same particles (or density), smaller, without rebuilding them.
    void drawSmall(sf::RenderTarget& target, const Moment& moment, Render render, const Theme& theme, float pixel) {
        const sf::Color accent = theme.resolve(ProgressBar::Fill).color;
        const sf::Color ink = theme.resolve(Paragraph::Heading).color;
        sf::RectangleShape box(boxSize);
        box.setFillColor(mixed(theme.resolve(Panel::Background).color, theme.resolve(Panel::Background).border, 0.12f));
        target.draw(box);
        target.draw(render == Render::Density ? static_cast<const sf::Drawable&>(m_density) : m_particles);
        drawMarkers(target, moment, accent, ink, pixel);
    }

private:
    void rebuildParticles(
        const Moment& moment, Render render, sf::Color accent, sf::Color ink, sf::Color slow, float pixel
    ) {
        const float size = std::max(2.5f, 2.f * pixel); // at least two pixels, whatever the zoom
        if (moment.tick == m_builtTick && render == m_builtRender && size == m_builtSize &&
            m_particles.size() == moment.positions.size()) {
            return;
        }
        m_builtTick = moment.tick;
        m_builtRender = render;
        m_builtSize = size;

        const Stopwatch build;
        const sf::Vector2u textureSize = m_particles.texture()->getSize();
        const FloatRect whole(0.f, 0.f, static_cast<float>(textureSize.x), static_cast<float>(textureSize.y));
        m_particles.resize(moment.positions.size());
        for (std::size_t i = 0; i < moment.positions.size(); ++i) {
            sf::Color color = accent;
            if (render == Render::Speed) {
                // Slow in the outline colour, then the accent, the fastest in the text colour.
                const float t = moment.speeds[i] / 160.f;
                color = t < 1.f ? mixed(slow, accent, t) : mixed(accent, ink, t - 1.f);
            }
            const sf::Vector2f p = moment.positions[i];
            m_particles.set(i, FloatRect(p.x - size * 0.5f, p.y - size * 0.5f, size, size), whole, color);
        }
        buildMilliseconds = build.milliseconds();
    }

    static void
    drawMarkers(sf::RenderTarget& target, const Moment& moment, sf::Color accent, sf::Color ink, float pixel) {
        const float radius = 10.f * std::max(pixel, 0.6f);
        sf::CircleShape mark(radius, 24);
        mark.setOrigin({ radius, radius });
        mark.setOutlineThickness(2.f * pixel);
        for (const Marker& marker : moment.markers) {
            mark.setPosition(marker.position);
            switch (marker.kind) {
                case Marker::Kind::Spawner: // a ring in the text colour
                    mark.setFillColor(sf::Color::Transparent);
                    mark.setOutlineColor(ink);
                    break;
                case Marker::Kind::Attractor: // filled in the accent
                    mark.setFillColor(accent);
                    mark.setOutlineColor(accent);
                    break;
                case Marker::Kind::Repulsor: // a ring in the accent
                    mark.setFillColor(sf::Color::Transparent);
                    mark.setOutlineColor(accent);
                    break;
            }
            target.draw(mark);
        }
    }

public:
    double buildMilliseconds = 0.0; ///< How long rebuilding the particles took last time.

private:
    QuadBatch m_particles;
    QuadBatch m_density;
    std::uint64_t m_builtTick = ~std::uint64_t{ 0 };
    Render m_builtRender = Render::Particles;
    float m_builtSize = 0.f;
};

// ----- Editing: forwarded input over the main view -----

class Editor {
public:
    Editor(Particles& simulation, Params& params, const Camera& camera) :
        m_simulation(simulation),
        m_params(params),
        m_camera(camera) {}

    /// Returns true if the event changed something.
    bool handle(const Event& event, const Moment& moment) {
        if (const auto* press = event.getIf<PointerPressed>(); press != nullptr && press->pointer.isIn("world")) {
            const sf::Vector2f at = m_camera.toWorld(press->pointer.inView);
            const std::optional<std::size_t> hit = markerAt(moment, at);
            if (press->button == sf::Mouse::Button::Right) {
                if (hit) {
                    m_simulation.send(Remove{ *hit });
                }
                return hit.has_value();
            }
            if (press->button != sf::Mouse::Button::Left) {
                return false;
            }
            switch (m_params.tool.get()) {
                case Tool::Pull:
                    pullAt(at);
                    m_params.pulling = true;
                    return true;
                case Tool::Erase:
                    if (hit) {
                        m_simulation.send(Remove{ *hit });
                    }
                    return hit.has_value();
                default:
                    if (hit) { // on a marker: drag it
                        m_dragging = *hit;
                    } else {
                        m_simulation.send(Place{ { kindOf(m_params.tool.get()), at } });
                        m_dragging = moment.markers.size(); // the one just placed
                    }
                    return true;
            }
        }
        if (const auto* move = event.getIf<PointerMoved>()) {
            const sf::Vector2f at = m_camera.toWorld(move->pointer.inView);
            if (m_params.pulling.get()) {
                pullAt(at);
            }
            if (m_dragging) {
                m_simulation.send(Move{ *m_dragging, at });
                return true;
            }
            return false;
        }
        if (event.is<PointerReleased>()) {
            m_params.pulling = false;
            m_dragging.reset();
        }
        return false;
    }

private:
    static Marker::Kind kindOf(Tool tool) {
        return tool == Tool::Spawner     ? Marker::Kind::Spawner
               : tool == Tool::Attractor ? Marker::Kind::Attractor
                                         : Marker::Kind::Repulsor;
    }

    /// The marker under the pointer, if any: within its drawn size and a little more.
    [[nodiscard]] std::optional<std::size_t> markerAt(const Moment& moment, sf::Vector2f at) const {
        const float grab = 14.f / m_camera.zoom() + 4.f;
        for (std::size_t i = moment.markers.size(); i-- > 0;) { // the topmost first
            if ((moment.markers[i].position - at).length() < grab) {
                return i;
            }
        }
        return std::nullopt;
    }

    void pullAt(sf::Vector2f at) {
        m_params.pullX = at.x;
        m_params.pullY = at.y;
    }

    Particles& m_simulation;
    Params& m_params;
    const Camera& m_camera;
    std::optional<std::size_t> m_dragging;
};

// ----- The application -----

int run(bool smokeTest) {
    Params params;
    Particles simulation(params);
    Param<double> buildMilliseconds;

    // The middle button moves the view, so that the left one is free for the tools.
    Camera camera("world", { .dragButton = sf::Mouse::Button::Middle });
    Minimap minimap("minimap", camera, FloatRect(sf::Vector2f(-20.f, -20.f), boxSize + sf::Vector2f(40.f, 40.f)));

    App app({
        .window = {.title = "atpl particles", .size = {1440u, 860u}},
        .ui = {
            .theme = themes::colorful(),
            .defaultView = "world",
            // Every panel in a cell of the window's grid: four columns, three rows.
            //
            //   Settings | Info  Info  | Minimap
            //   Settings | World World World
            //   Analytics| World World World
            .grid = {.columns = 4, .rows = 3},
            .panels = {
                {
                    .name = "Settings",
                    .placement = GridCell{.column = 0, .row = 0, .rowSpan = 2},
                    .columns = 2,
                    .collapsible = false,
                    .widgets = {
                        // Left: the world. Right: what you do with it, and how it is shown.
                        Dropdown("Spawn", {layoutNames.begin(), layoutNames.end()}, params.layout),
                        Slider("Count", params.count, {.min = 20000.0, .max = 100000.0, .step = 5000.0, .format = "{:.0f}"}),
                        Dropdown("Gravity", {gravityNames.begin(), gravityNames.end()}, params.gravity),
                        Slider("Strength", params.gravityStrength, {.min = 0.0, .max = 300.0, .format = "{:.0f}"}),
                        Slider("Push", params.push, {.min = 0.0, .max = 1000.0, .format = "{:.0f}"}),
                        Slider("Friction", params.friction, {.min = 0.0, .max = 2.0, .format = "{:.2f}"}),
                        Button("Respawn"),
                        Dropdown("Tool", {toolNames.begin(), toolNames.end()}, params.tool),
                        Button("Clear markers"),
                        Dropdown("Render", {renderNames.begin(), renderNames.end()}, params.render),
                        Slider("Speed", simulation.controls.speed, {.min = 0.0, .max = 3.0, .format = "{:.2f}"}),
                        Switch("Paused", simulation.controls.paused),
                        Button("Step"),
                    },
                },
                {
                    .name = "Particles",
                    .placement = GridCell{.column = 1, .row = 0, .columnSpan = 2},
                    .columns = 2,
                    .collapsible = false,
                    .widgets = {
                        at({.column = 0, .row = 0}, Paragraph("About", {
                                            .text = "Tens of thousands of particles on all cores. They push each other apart, fall to the centre or down, and follow what you place.",
                                            .footer = "Pull: hold the left button. Spawner, Attractor, Repulsor: click to place, drag to "
                                                      "move. Right button: remove. Middle button: move the view. Escape quits."})),
                        at({.column = 1, .row = 0}, Graph("Kinetic energy", simulation.energy)),
                    },
                },
                {
                    .name = "Map",
                    .placement = GridCell{.column = 3, .row = 0},
                    .collapsible = false,
                    .widgets = { View("minimap") },
                },
                {
                    .name = "Analytics",
                    .placement = GridCell{.column = 0, .row = 2},
                    .columns = 2,
                    .collapsible = false,
                    .widgets = {
                        at({.column = 0, .row = 0}, ValueDisplay("Ticks per second", simulation.controls.ticksPerSecond, {.format = "{:.0f}"})),
                        at({.column = 1, .row = 0}, ValueDisplay("Tick time", simulation.controls.tickMilliseconds, {.format = "{:.1f} ms"})),
                        at({.column = 0, .row = 1}, ValueDisplay("In parallelFor", simulation.parallelShare, {.format = "{:.0f} %"})),
                        at({.column = 1, .row = 1}, ValueDisplay("Drawing", buildMilliseconds, {.format = "{:.1f} ms"})),
                        at({.column = 0, .row = 2}, ValueDisplay("Particles", simulation.particleCount, {.format = "{:.0f}"})),
                    },
                },
                {
                    .name = "World",
                    .placement = GridCell{.column = 1, .row = 1, .columnSpan = 3, .rowSpan = 2},
                    .collapsible = false,
                    .widgets = { View("world") },
                },
            },
        },
        .scale = AutoScale{},
    });
    camera.show({ -20.f, -20.f }, boxSize + sf::Vector2f(40.f, 40.f));

    WorldPainter painter(app.resources().texture("textures/particle.png"));
    Editor editor(simulation, params, camera);

    app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        painter.draw(
            target, simulation.state(), params.render.get(), app.ui().theme(), app.ui().scale() / camera.zoom()
        );
        buildMilliseconds = painter.buildMilliseconds;
    });
    app.ui().view("minimap").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        minimap.apply(target, size);
        const float pixel = minimap.world().width() / size.x;
        painter.drawSmall(target, simulation.state(), params.render.get(), app.ui().theme(), pixel);
        minimap.drawMarks(target, app.ui().theme().resolve(Paragraph::Heading).color);
    });

    app.onEvent([&](const Event& event) {
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }
        if (event.isButton("Respawn")) {
            simulation.send(Respawn{});
        }
        if (event.isButton("Clear markers")) {
            simulation.send(ClearMarkers{});
        }
        if (event.isButton("Step")) {
            simulation.controls.paused = true;
            simulation.controls.step();
        }
        // A new layout or count: spawn again.
        for (const std::string_view name : { "Spawn", "Count" }) {
            if (const ValueChanged* changed = event.changeOf(name); changed != nullptr && changed->final) {
                simulation.send(Respawn{});
            }
        }
        const bool edited = editor.handle(event, simulation.state());
        const bool mapMoved = minimap.handle(event);
        const bool viewMoved = camera.handle(event);
        if (edited || mapMoved || viewMoved) {
            app.ui().requestRedraw();
        }
    });

    int passesLeft = 5; // only counted in a smoke test
    if (smokeTest) {
        app.onUpdate([&](float) {
            app.ui().requestRedraw();
            if (--passesLeft == 0) {
                app.quit();
            }
        });
    }
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
