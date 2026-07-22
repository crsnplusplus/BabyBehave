#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

bool GivenRegisteredStep(TestContext& ctx) {
    ctx.Set(Key<bool>{"flag"}, true);
    return true;
}

} // namespace

int main() {
    StepRegistry registry;

    // Register only one step
    registry.RegisterGiven("a registered step", GivenRegisteredStep);

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
    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinUnmatchedStep.cpp").Run();
    return result.ExitCode();
}
