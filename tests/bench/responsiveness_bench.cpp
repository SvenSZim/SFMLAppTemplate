// Checks the promise of Phase 4: the UI stays at display rate and keeps handling input while the
// simulation is slow, because the two run on threads of their own (docs/PROJECT_PLAN.md 5.5).
//
// An application runs a simulation whose every tick keeps a core busy for `--tick-ms`, and asks
// for a frame in every pass of its loop. It records how long each pass takes: the longest the
// UI ever goes without reading input. With vsync on, a pass is one display frame.
//
//   atpl_responsiveness_bench [--seconds N] [--tick-ms N]
//
// The exit code says whether the UI kept going: at least ten passes for every tick, and at least
// one tick. Times are printed, not judged: they depend on the machine and the display.

#include "atpl/app/app.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

using namespace atpl;

/// A simulation that does nothing useful, slowly: every tick keeps the thread busy for the given
/// time, as heavy simulation code would.
class Slow final : public Simulation<int> {
public:
    explicit Slow(std::chrono::milliseconds cost) :
        m_cost(cost) {}

private:
    void tick(float /*dt*/) override {
        const auto until = std::chrono::steady_clock::now() + m_cost;
        while (std::chrono::steady_clock::now() < until) {
            // busy, not asleep: a core is taken
        }
        ++m_ticks;
    }
    void writeState(int& state) const override { state = m_ticks; }

    std::chrono::milliseconds m_cost;
    int m_ticks = 0;
};

/// The value below which `share` of the sorted values lie.
double percentile(const std::vector<double>& sorted, double share) {
    if (sorted.empty()) {
        return 0.0;
    }
    const auto index = static_cast<std::size_t>(share * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

} // namespace

int main(int argc, char* argv[]) {
    double seconds = 10.0;
    int tickMs = 1000;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const bool hasValue = i + 1 < argc;
        if (argument == "--seconds" && hasValue) {
            seconds = std::atof(argv[++i]);
        } else if (argument == "--tick-ms" && hasValue) {
            tickMs = std::atoi(argv[++i]);
        } else {
            std::fprintf(stderr, "usage: atpl_responsiveness_bench [--seconds N] [--tick-ms N]\n");
            return 2;
        }
    }

    try {
        Slow simulation{ std::chrono::milliseconds(tickMs) };
        simulation.controls.tickRate = 1.0; // one tick due per second; each takes `tickMs`

        App app({
            .window = {.title = "atpl responsiveness bench", .size = { 960u, 540u }},
            .ui = {
                .panels = {
                    {
                        .name = "Simulation",
                        .widgets = {
                            ValueDisplay("Ticks", simulation.controls.tickCount, {.format = "{:.0f}"}),
                            ValueDisplay("Tick time (ms)", simulation.controls.tickMilliseconds, {.format = "{:.0f}"}),
                            Slider("Speed", simulation.controls.speed, {.min = 0.0, .max = 4.0}),
                        },
                    },
                },
                .profiler = true,
            },
        });

        // Every pass asks for a frame, so the loop runs at display rate; `dt` is how long the
        // last pass took.
        std::vector<double> passes;
        double elapsed = 0.0;
        app.onUpdate([&](float dt) {
            passes.push_back(static_cast<double>(dt) * 1000.0);
            elapsed += dt;
            app.ui().requestRedraw();
            if (elapsed >= seconds) {
                app.quit();
            }
        });
        app.run(simulation);

        const double ticks = simulation.controls.tickCount.get();
        std::vector<double> sorted = passes;
        std::sort(sorted.begin(), sorted.end());
        std::printf("tick time:           %d ms\n", tickMs);
        std::printf("ticks:               %.0f in %.1f s\n", ticks, elapsed);
        std::printf(
            "passes of the loop:  %zu (%.1f per second)\n", passes.size(), static_cast<double>(passes.size()) / elapsed
        );
        std::printf("pass time, median:   %.2f ms\n", percentile(sorted, 0.5));
        std::printf("pass time, 99 %%:     %.2f ms\n", percentile(sorted, 0.99));
        std::printf("pass time, longest:  %.2f ms\n", sorted.empty() ? 0.0 : sorted.back());

        const bool keptGoing = ticks >= 1.0 && static_cast<double>(passes.size()) >= 10.0 * std::max(ticks, 1.0);
        std::printf("%s\n", keptGoing ? "the UI kept going" : "the UI did NOT keep going");
        return keptGoing ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
