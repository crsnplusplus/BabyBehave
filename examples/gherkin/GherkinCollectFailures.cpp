#include <BabyBehave/bdd.hpp>
#include <iostream>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

constexpr Key<int> kTestValue{"test_value"};

bool GivenTestValue(TestContext& ctx, int value) {
    ctx.Set(kTestValue, value);
    std::cout << "  [Given] Set test value to " << value << std::endl;
    return true;
}

bool WhenExpectItToEqual(TestContext& ctx, int expected) {
    const int value = ctx.Get(kTestValue);
    const bool passes = (value == expected);
    if (passes) {
        std::cout << "  [When] Value matches " << expected << std::endl;
    } else {
        std::cout << "  [When] Value " << value << " does not match " << expected << std::endl;
    }
    return passes;
}

bool ThenVerifyTheValueAgain(TestContext& ctx) {
    // This step always passes - to show that subsequent steps still run
    // even after an earlier step failed
    std::cout << "  [Then] Verifying value (this runs even after failure)" << std::endl;
    return true;
}

bool AndDoubleCheckTheValue(TestContext& ctx) {
    // This also runs, demonstrating that all steps run despite the failure
    std::cout << "  [And] Double-checking (And step after failure)" << std::endl;
    return true;
}

} // namespace

int main() {
    StepRegistry registry;

    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a test value {int}", GivenTestValue},
        StepEntry{Keyword::When, "I expect it to equal {int}", WhenExpectItToEqual},
        StepEntry{Keyword::Then, "I verify the value again", ThenVerifyTheValueAgain},
        StepEntry{Keyword::And, "I double-check the value", AndDoubleCheckTheValue});

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
    // (default onFailure - unchanged).
    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinCollectFailures.cpp").Run();

    // This code never executes - the default onFailure exits after scenario failure
    return result.ExitCode();
}
