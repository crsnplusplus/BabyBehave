// test_SelfTest_Gherkin_DefaultFailureExit.cpp
//
// A single-purpose BBH probe, Gherkin edition: unlike every scenario in
// test_SelfTest_Gherkin.cpp, this one calls RunFeature() with NO custom
// GherkinFailureCallback, so the genuine default
// (Gherkin::impl::DefaultGherkinFailureAction - print to stderr, then
// std::exit(EXIT_FAILURE)) fires for real on the deliberately-failing
// Scenario below. That default path is otherwise unreachable in the BBH
// suite, since every other Gherkin scenario file installs a collecting
// callback so later scenarios can still run afterward - mirrors
// test_SelfTest_DefaultConditionExit.cpp's rationale exactly.
//
// This process is EXPECTED to exit with a non-zero status; it is
// registered in tests/bdd/CMakeLists.txt with WILL_FAIL TRUE so ctest
// treats that as a pass.

#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {
bool AlwaysFalse(TestContext&) { return false; }
} // namespace

int main() {
    StepRegistry registry;
    registry.RegisterGiven("nothing relevant", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("this always fails", AlwaysFalse);

    constexpr std::string_view feature = R"feature(
Feature: Default Gherkin failure handling
  Scenario: Deliberately failing, no custom callback installed
    Given nothing relevant
    Then this always fails
)feature";

    // No GherkinFailureCallback argument here on purpose: RunFeature() runs
    // the Scenario above, sees allPassed==false, and invokes
    // impl::DefaultGherkinFailureAction -> exits the process before
    // returning.
    (void)RunFeature(feature, registry, "SelfTestGherkinDefaultFailureExit");

    // Unreachable: the line above exits the process before this point.
    return EXIT_SUCCESS;
}
