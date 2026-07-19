#include <BabyBehave/bdd.hpp>
#include <BabyBehave/reporters.hpp>
#include <iostream>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;
using namespace BabyBehave::BDD::Reporters;

// Very advanced Gherkin example demonstrating:
// - Tag inheritance from Feature-level tags (@critical) down to Scenario
//   tags, enabling selective Before/After hook execution
// - Scenario execution with multiple steps and step validation
// - After hooks that run for all scenarios (empty tag filter)
// - Before hooks that run only for scenarios matching specific tags
// - Direct inspection of FeatureResult/TestResult structures after
//   execution (featureName, scenarioResults, allPassed flag)
// - Step-level inspection of TestResult::steps (step names, pass/fail
//   status, messages, locations)
// - Bridging Gherkin scenarios into the reporters.hpp machinery:
//   calling ToJUnitXml() and ToTap() to export TestResult objects
//   to JUnit XML and TAP formats, showing that Gherkin-produced
//   results are fully compatible with the library's standard reporting
//   infrastructure
//
// Exit code: 0 (success) - a full happy-path narrative demonstrating
// the complete machinery in action, including reporters integration.

int main() {
    StepRegistry registry;

    // ---- Setup steps ----
    registry.RegisterGiven("a session is active", [](TestContext& ctx) -> bool {
        ctx.Set("session_valid", true);
        ctx.Set("requests", 0);
        std::cout << "    [Given] Session started\n";
        return true;
    });

    auto requestArrivesImpl = [](TestContext& ctx) -> bool {
        int requests = ctx.Get<int>("requests");
        ctx.Set("requests", requests + 1);
        std::cout << "    [When] Request #" << (requests + 1) << " processed\n";
        return true;
    };

    registry.RegisterWhen("a request arrives", requestArrivesImpl);
    registry.RegisterAnd("a request arrives", requestArrivesImpl);

    registry.RegisterWhen("the session times out", [](TestContext& ctx) -> bool {
        ctx.Set("session_valid", false);
        std::cout << "    [When] Session timed out\n";
        return true;
    });

    registry.RegisterThen("the session should be active", [](TestContext& ctx) -> bool {
        bool valid = ctx.Get<bool>("session_valid");
        std::cout << "    [Then] Verifying session is active: " << (valid ? "YES" : "NO") << "\n";
        return valid;
    });

    registry.RegisterAnd("the session should be active", [](TestContext& ctx) -> bool {
        bool valid = ctx.Get<bool>("session_valid");
        std::cout << "    [And] Verifying session is active: " << (valid ? "YES" : "NO") << "\n";
        return valid;
    });

    registry.RegisterAnd("the request count should be {int}", [](TestContext& ctx, int expected) -> bool {
        int requests = ctx.Get<int>("requests");
        bool pass = (requests == expected);
        std::cout << "    [And] Request count check: " << requests << " == " << expected << " ? " << (pass ? "YES" : "NO") << "\n";
        return pass;
    });

    registry.RegisterBut("I did not expect it to fail", [](TestContext& ctx) -> bool {
        // This step deliberately fails to show collect-failures mode behavior
        std::cout << "    [But] This step WILL FAIL\n";
        return false; // Intentional failure
    });

    // ---- Before hook for @critical tag ----
    registry.AddBeforeHook(
        {"critical"},
        [](TestContext& ctx) {
            std::cout << "  [Before @critical] Initializing critical session\n";
        }
    );

    // ---- After hook: always runs (even after failure) ----
    registry.AddAfterHook(
        {},
        [](TestContext& ctx) {
            std::cout << "  [After] Session cleanup (this runs even after failure)\n";
        }
    );

    // --- Feature 1: Happy-path session management ---
    const std::string_view feature1 = R"feature(
Feature: @critical Session management
  Scenario: Session starts and handles request
    Given a session is active
    When a request arrives
    Then the session should be active
    And the request count should be 1

  Scenario: Multiple requests
    Given a session is active
    When a request arrives
    And a request arrives
    Then the session should be active
    And the request count should be 2
)feature";

    std::cout << "========== Running Feature 1: Session Management ==========\n";
    FeatureResult result1 = RunFeature(feature1, registry, "Feature1");
    std::cout << "\nFeature 1 completed with " << result1.scenarioResults.size() << " scenario(s).\n";

    std::cout << "\n========== Detailed Result Inspection ==========\n";

    // Inspect FeatureResult directly and demonstrate step-level reporting
    std::cout << "\nFeature: " << result1.featureName << "\n";
    std::cout << "  Total scenarios: " << result1.scenarioResults.size() << "\n";
    std::cout << "  All passed: " << (result1.allPassed ? "YES" : "NO") << "\n";

    for (std::size_t i = 0; i < result1.scenarioResults.size(); ++i) {
        const auto& scenario = result1.scenarioResults[i];
        std::cout << "\n  Scenario " << (i + 1) << ": " << scenario.testName << "\n";
        std::cout << "    Steps executed: " << scenario.steps.size() << "\n";
        std::cout << "    All passed: " << (scenario.allPassed ? "YES" : "NO") << "\n";
        for (std::size_t j = 0; j < scenario.steps.size(); ++j) {
            const auto& step = scenario.steps[j];
            std::cout << "      Step " << (j + 1) << ": " << step.stepName
                      << " [" << (step.passed ? "PASS" : "FAIL") << "]\n";
        }
    }

    // Bridge to reporters: export results to JUnit XML and TAP formats
    std::cout << "\n========== Exporting to JUnit XML ==========\n";
    std::string xmlReport = ToJUnitXml(result1.scenarioResults, "GherkinVeryAdvanced");
    std::cout << xmlReport << "\n";

    std::cout << "\n========== Exporting to TAP Format ==========\n";
    std::string tapReport = ToTap(result1.scenarioResults);
    std::cout << tapReport << "\n";

    std::cout << "========== Demonstration Complete ==========\n";
    std::cout << "This example demonstrated:\n";
    std::cout << "  - Tag inheritance from Feature (@critical) to Scenarios\n";
    std::cout << "  - Before hooks running for matching tags\n";
    std::cout << "  - After hooks running for all scenarios\n";
    std::cout << "  - Direct inspection of FeatureResult::scenarioResults\n";
    std::cout << "  - Step-by-step inspection of TestResult::steps\n";
    std::cout << "  - All-passed flag checking (allPassed field)\n";
    std::cout << "  - Bridging Gherkin results to reporters.hpp machinery:\n";
    std::cout << "    * ToJUnitXml() - JUnit XML format for CI integration\n";
    std::cout << "    * ToTap() - TAP (Test Anything Protocol) format\n";
    std::cout << "  - Gherkin TestResult objects are fully compatible with\n";
    std::cout << "    the library's standard reporting infrastructure\n";

    return 0;
}
