#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    // Shared step definitions
    registry.RegisterGiven("a simple counter", [](TestContext& ctx) -> bool {
        ctx.Set("counter", 0);
        return true;
    });

    registry.RegisterWhen("I increment the counter", [](TestContext& ctx) -> bool {
        int value = ctx.Get<int>("counter");
        ctx.Set("counter", value + 1);
        return true;
    });

    registry.RegisterWhen("I increment the counter {int} times", [](TestContext& ctx, int times) -> bool {
        int value = ctx.Get<int>("counter");
        ctx.Set("counter", value + times);
        return true;
    });

    registry.RegisterThen("the counter should be {int}", [](TestContext& ctx, int expected) -> bool {
        int value = ctx.Get<int>("counter");
        return value == expected;
    });

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

    const auto result = RunFeature(feature, registry, "examples/GherkinBackground.cpp");
    return result.allPassed ? 0 : 1;
}
