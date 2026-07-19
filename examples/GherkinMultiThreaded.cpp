#include <BabyBehave/bdd.hpp>
#include <future>
#include <thread>
#include <vector>
#include <iostream>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Demonstrates thread-safe Gherkin scenario execution using std::async.
// Each thread constructs its OWN StepRegistry and runs its OWN RunFeature()
// call with its own embedded feature text. Because there is zero shared
// mutable state between scenarios, launching N of them in parallel needs
// no locks, no atomics, no synchronization at all.
//
// This mirrors the pattern from examples/MultiThreaded.cpp but using Gherkin
// instead of the fluent API.

namespace {

// Thread-safe helper to run a single Gherkin feature scenario
// Each thread builds its own registry and feature text independently
void RunCountingFeature(int threadId) {
    StepRegistry registry;

    registry.RegisterGiven("a counter at {int}", [](TestContext& ctx, int value) -> bool {
        ctx.Set("counter", value);
        return true;
    });

    registry.RegisterWhen("I add {int}", [](TestContext& ctx, int value) -> bool {
        int current = ctx.Get<int>("counter");
        ctx.Set("counter", current + value);
        return true;
    });

    registry.RegisterAnd("I add {int}", [](TestContext& ctx, int value) -> bool {
        int current = ctx.Get<int>("counter");
        ctx.Set("counter", current + value);
        return true;
    });

    registry.RegisterThen("the counter equals {int}", [](TestContext& ctx, int expected) -> bool {
        int current = ctx.Get<int>("counter");
        return current == expected;
    });

    registry.RegisterAnd("the counter equals {int}", [](TestContext& ctx, int expected) -> bool {
        int current = ctx.Get<int>("counter");
        return current == expected;
    });

    // Each thread uses different feature text to verify independent execution
    std::string feature;
    if (threadId == 0) {
        feature = R"feature(
Feature: Counter thread 0
  Scenario: Basic counting
    Given a counter at 10
    When I add 5
    Then the counter equals 15
)feature";
    } else if (threadId == 1) {
        feature = R"feature(
Feature: Counter thread 1
  Scenario: Multiple additions
    Given a counter at 0
    When I add 3
    And I add 7
    Then the counter equals 10
)feature";
    } else if (threadId == 2) {
        feature = R"feature(
Feature: Counter thread 2
  Scenario: Larger numbers
    Given a counter at 100
    When I add 50
    And I add 25
    Then the counter equals 175
)feature";
    } else {
        feature = R"feature(
Feature: Counter thread 3
  Scenario: Negative addition
    Given a counter at 20
    When I add -5
    Then the counter equals 15
)feature";
    }

    std::string label = "thread-" + std::to_string(threadId);
    const auto result = RunFeature(feature, registry, label);
    if (!result.allPassed) {
        std::cerr << "Thread " << threadId << " failed!" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    constexpr int kThreadCount = 4;

    std::vector<std::future<void>> futures;
    futures.reserve(kThreadCount);

    // Launch 4 independent Gherkin scenarios, each in its own thread
    // Each thread runs with its own StepRegistry and feature text
    for (int i = 0; i < kThreadCount; ++i) {
        futures.push_back(std::async(std::launch::async, RunCountingFeature, i));
    }

    // Wait for all threads to complete
    for (auto& fut : futures) {
        fut.get();
    }

    std::cout << kThreadCount << " independent Gherkin scenarios completed concurrently, "
              << "each with its own StepRegistry and feature text -- no synchronization needed.\n";

    return 0;
}
