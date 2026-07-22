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

namespace {

constexpr Key<double> kOvenTemperature{"oven_temperature"};
constexpr Key<bool> kSensorStable{"sensor_stable"};
constexpr Key<int> kSensorReadAttempts{"sensor_read_attempts"};

// ---- Flaky sensor setup ----
bool GivenOvenIsWarmingUp(TestContext& ctx) {
    ctx.Set(kOvenTemperature, 0.0);
    ctx.Set(kSensorStable, false); // Initially not stable; will be set to true on successful read
    ctx.Set(kSensorReadAttempts, 0); // Track read attempts within this scenario
    return true;
}

// ---- Flaky temperature read (fails first 2 times, succeeds 3rd) ----
bool WhenReadOvenTemperature(TestContext& ctx) {
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

    // Track the attempt number for diagnostics/logging
    ctx.Mutate(kSensorReadAttempts) += 1;

    // Simulate a flaky sensor: fail on attempts 1-2, succeed on attempt 3+
    if (read_attempt < 3) {
        // Simulate transient sensor glitch (e.g., bad read, intermittent connection)
        return false;
    }

    // On attempt 3, sensor recovers and returns a stable reading
    ctx.Set(kOvenTemperature, 350.0); // Valid preheat temperature
    ctx.Set(kSensorStable, true);
    return true;
}

// ---- Assertion ----
bool ThenTemperatureReadingShouldBeStableAndInRange(TestContext& ctx) {
    const double temp = ctx.Get(kOvenTemperature);
    const bool stable = ctx.Get(kSensorStable);
    // Preheat range: 300-400°F is reasonable for bakery ovens
    return stable && temp >= 300.0 && temp <= 400.0;
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry;
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "the oven is warming up", GivenOvenIsWarmingUp},
        StepEntry{Keyword::When, "I read the oven temperature", WhenReadOvenTemperature},
        StepEntry{Keyword::Then, "the temperature reading should be stable and within range",
            ThenTemperatureReadingShouldBeStableAndInRange});
    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_flaky_oven_sensor_retry.feature");
}
