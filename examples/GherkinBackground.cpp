#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

constexpr Key<int> kCounter{"counter"};

bool GivenSimpleCounter(TestContext& ctx) {
    ctx.Set(kCounter, 0);
    return true;
}

bool WhenIncrementCounter(TestContext& ctx) {
    ctx.Mutate(kCounter) += 1;
    return true;
}

bool WhenIncrementCounterTimes(TestContext& ctx, int times) {
    ctx.Mutate(kCounter) += times;
    return true;
}

bool ThenCounterShouldBe(TestContext& ctx, int expected) {
    return ctx.Get(kCounter) == expected;
}

} // namespace

int main() {
    StepRegistry registry;

    // Shared step definitions
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a simple counter", GivenSimpleCounter},
        StepEntry{Keyword::When, "I increment the counter", WhenIncrementCounter},
        StepEntry{Keyword::When, "I increment the counter {int} times", WhenIncrementCounterTimes},
        StepEntry{Keyword::Then, "the counter should be {int}", ThenCounterShouldBe});

    const std::string_view feature = R"feature(
Feature: Counter with background
  Background:
    Given a simple counter
    When I increment the counter

  Scenario: One more increment reaches two
    When I increment the counter
    Then the counter should be 2

  Scenario: Increment multiple times
    When I increment the counter 3 times
    Then the counter should be 4
)feature";

    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinBackground.cpp").Run();
    return result.ExitCode();
}
