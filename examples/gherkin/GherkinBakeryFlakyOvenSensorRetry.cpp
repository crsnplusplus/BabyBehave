#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <string>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Retry example: a bakery oven temperature sensor that is realistically "flaky",
// failing the first couple of read attempts before returning a stable, correct reading.
// Uses a static counter (deterministic, no randomness) to simulate the transient failures.
// The @retry:3 tag enables the retry mechanism - each attempt is a full re-run of
// Before hooks + Background + Steps + After hooks.
//
// The scenario text lives in examples/gherkin/features/bakery_flaky_oven_sensor_retry.feature,
// read from disk via LoadFeatureFile().

StepRegistry PrepareRegistry() {
    StepRegistry registry;

    // ---- Flaky sensor setup ----
    registry.RegisterGiven("the oven is warming up", [](TestContext& ctx) -> bool {
        ctx.Set("oven_temperature", 0.0);
        ctx.Set("sensor_stable", false);  // Initially not stable; will be set to true on successful read
        ctx.Set("sensor_read_attempts", 0);  // Track read attempts within this scenario
        return true;
    });

    // ---- Flaky temperature read (fails first 2 times, succeeds 3rd) ----
    registry.RegisterWhen("I read the oven temperature", [](TestContext& ctx) -> bool {
        // Static counter: initialized once and persists for the lifetime of
        // this process (it does NOT reset per retry attempt - a function-local
        // static never does). This is exactly why it works here: @retry:3 calls
        // this step once per full attempt (fresh TestContext each time, per
        // Feature 6's re-run-everything design), so read_attempt monotonically
        // counts total invocations across all attempts, reaching 3 on the final
        // (3rd) attempt - deterministic because it depends only on invocation
        // count, never on wall-clock time or randomness.
        static int read_attempt = 0;
        ++read_attempt;

        // Store the attempt number for diagnostics/logging
        int attempts = ctx.Get<int>("sensor_read_attempts");
        attempts++;
        ctx.Set("sensor_read_attempts", attempts);

        // Simulate a flaky sensor: fail on attempts 1-2, succeed on attempt 3+
        if (read_attempt < 3) {
            // Simulate transient sensor glitch (e.g., bad read, intermittent connection)
            return false;
        }

        // On attempt 3, sensor recovers and returns a stable reading
        ctx.Set("oven_temperature", 350.0);  // Valid preheat temperature
        ctx.Set("sensor_stable", true);
        return true;
    });

    // ---- Assertion ----
    registry.RegisterThen("the temperature reading should be stable and within range",
        [](TestContext& ctx) -> bool {
            double temp = ctx.Get<double>("oven_temperature");
            bool stable = ctx.Get<bool>("sensor_stable");
            // Preheat range: 300-400°F is reasonable for bakery ovens
            return stable && temp >= 300.0 && temp <= 400.0;
        });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_flaky_oven_sensor_retry.feature");
}
