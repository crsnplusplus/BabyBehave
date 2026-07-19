#include <BabyBehave/bdd.hpp>
#include <iostream>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    registry.RegisterGiven("a test value {int}", [](TestContext& ctx, int value) -> bool {
        ctx.Set("test_value", value);
        std::cout << "  [Given] Set test value to " << value << std::endl;
        return true;
    });

    registry.RegisterWhen("I expect it to equal {int}", [](TestContext& ctx, int expected) -> bool {
        int value = ctx.Get<int>("test_value");
        bool passes = (value == expected);
        if (passes) {
            std::cout << "  [When] Value matches " << expected << std::endl;
        } else {
            std::cout << "  [When] Value " << value << " does not match " << expected << std::endl;
        }
        return passes;
    });

    registry.RegisterThen("I verify the value again", [](TestContext& ctx) -> bool {
        // This step always passes - to show that subsequent steps still run
        // even after an earlier step failed
        std::cout << "  [Then] Verifying value (this runs even after failure)" << std::endl;
        return true;
    });

    registry.RegisterAnd("I double-check the value", [](TestContext& ctx) -> bool {
        // This also runs, demonstrating that all steps run despite the failure
        std::cout << "  [And] Double-checking (And step after failure)" << std::endl;
        return true;
    });

    const std::string_view feature = R"feature(
Feature: Collect failures to see all step outcomes
  Scenario: Middle step fails but others still run
    Given a test value 5
    When I expect it to equal 10
    Then I verify the value again
    And I double-check the value
)feature";

    std::cout << "Running Gherkin scenario with middle step failure:\n" << std::endl;
    std::cout << "NOTE: All steps run despite middle step failure:\n" << std::endl;
    std::cout << "  - First step passes (Given)\n" << std::endl;
    std::cout << "  - Middle step fails (When: 5 != 10)\n" << std::endl;
    std::cout << "  - Remaining steps still execute (Then, And)\n" << std::endl;
    std::cout << "  - Full results collected before scenario failure\n" << std::endl;

    // This will call exit(EXIT_FAILURE) after collecting all step results
    const auto result = RunFeature(feature, registry, "examples/GherkinCollectFailures.cpp");

    // This code never executes - RunFeature exits after scenario failure
    return result.allPassed ? 0 : 1;
}
