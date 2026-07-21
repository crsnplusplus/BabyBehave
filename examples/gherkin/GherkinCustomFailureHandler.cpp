#include <BabyBehave/bdd.hpp>
#include <iostream>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Demonstrates the v0.8.1 onFailureCallback extension point on RunFeature().
//
// By default RunFeature() fails hard: any parse error or failing Scenario
// prints a diagnostic and calls std::exit(EXIT_FAILURE) immediately (see
// GherkinUnmatchedStep.cpp/GherkinCollectFailures.cpp). That's the right
// default for a small standalone binary, but an advanced consumer plugging
// Gherkin into a larger test harness (their own reporter, CI aggregation,
// etc.) may want to redirect those failures instead of crashing the process
// outright. Passing a custom GherkinFailureCallback as RunFeature()'s fourth
// argument does exactly that.
//
// A GherkinFailureCallback (std::function<void(std::string_view)>, not
// move_only_function) may be invoked more than once per RunFeature() call -
// once for a parse error, or once per failing Scenario - so this example
// collects every message into a std::vector<std::string> rather than
// assuming a single call.
//
// Because our callback here does NOT exit or throw, RunFeature() keeps going
// after a failing Scenario instead of stopping at the first one (this is
// the "collect Gherkin failures across the whole feature" mode the design
// doc describes) and returns a FeatureResult with allPassed==false once
// every Scenario has run. This example inspects that at the end and decides
// its own process exit code, rather than letting RunFeature() decide for it.
int main() {
    StepRegistry registry;

    registry.RegisterGiven("a counter starting at {int}", [](TestContext& ctx, int start) -> bool {
        ctx.Set("counter", start);
        return true;
    });
    registry.RegisterWhen("I increment the counter by {int}", [](TestContext& ctx, int delta) -> bool {
        ctx.Set("counter", ctx.Get<int>("counter") + delta);
        return true;
    });
    registry.RegisterThen("the counter should be {int}", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("counter") == expected;
    });

    const std::string_view feature = R"feature(
Feature: Custom failure handling
  Scenario: A passing scenario
    Given a counter starting at 1
    When I increment the counter by 2
    Then the counter should be 3

  Scenario: A failing scenario
    Given a counter starting at 1
    When I increment the counter by 2
    Then the counter should be 100
)feature";

    // Collect every Gherkin-sourced failure message instead of letting
    // RunFeature() print-and-exit for us.
    std::vector<std::string> collectedFailures;
    const GherkinFailureCallback collectFailures = [&collectedFailures](std::string_view message) {
        collectedFailures.emplace_back(message);
    };

    const auto result = RunFeature(feature, registry, "examples/GherkinCustomFailureHandler.cpp", collectFailures);

    std::cout << "Scenarios run: " << result.scenarioResults.size() << '\n';
    std::cout << "Failures collected by our custom callback: " << collectedFailures.size() << '\n';
    for (const auto& message : collectedFailures) {
        std::cout << "  - " << message << '\n';
    }

    // The consumer decides the process's fate, not RunFeature() itself: here
    // we mirror the library's own fail-hard convention by exiting non-zero
    // when any Scenario failed, but a real harness could just as easily log
    // this and keep going (e.g. to run more features before reporting).
    return result.allPassed ? 0 : 1;
}
