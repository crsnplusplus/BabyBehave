#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    // Register only one step
    registry.RegisterGiven("a registered step", [](TestContext& ctx) -> bool {
        ctx.Set("flag", true);
        return true;
    });

    // Note: NO registration for "an unregistered step" - it will fail
    // because no step definition matches.

    const std::string_view feature = R"feature(
Feature: Unmatched step failure
  Scenario: Step with no matching definition
    Given a registered step
    When an unregistered step
    Then this should not run
)feature";

    // This will call std::exit(EXIT_FAILURE) due to the unmatched step
    const auto result = RunFeature(feature, registry, "examples/GherkinUnmatchedStep.cpp");
    return result.allPassed ? 0 : 1;
}
