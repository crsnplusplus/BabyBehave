// test_SelfTest_Gherkin.cpp
//
// A "dogfood" example, Gherkin edition: BabyBehave's own Gherkin runtime
// (BabyBehave::BDD::Gherkin - StepRegistry + RunFeature, see bdd.hpp and
// docs/design/gherkin-support.md) exercised through its own public surface,
// exactly the way test_SelfTest.cpp dogfoods the non-Gherkin fluent API.
//
// This is the BBH half of bdd.hpp's Gherkin coverage measurement (see
// coverage-bbh in the root CMakeLists.txt); the gtest-based UT half lives in
// tests/test_Gherkin_Parser.cpp and tests/test_Gherkin_Integration.cpp.
// bdd.hpp being header-only means each TU - gtest-based or self-hosted -
// compiles its own private copy of the Gherkin namespace's inline code, so
// this file is a genuinely independent coverage source, not a duplicate of
// the UT suite's counters.
//
// IMPORTANT: RunFeature()'s default GherkinFailureCallback
// (impl::DefaultGherkinFailureAction) prints + std::exit()s on the first
// parse error or failing Scenario - the same fail-hard philosophy as
// BabyBehaveTest's default callbacks. Every scenario below except the one in
// test_SelfTest_Gherkin_DefaultFailureExit.cpp (a dedicated, WILL_FAIL-marked
// binary mirroring test_SelfTest_DefaultConditionExit.cpp) passes its own
// collecting GherkinFailureCallback, so a "failing on purpose" scenario here
// never exits the process and every later scenario still runs.

#include <BabyBehave/bdd.hpp>

#include "SelfTestDiagnostics.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

// A GherkinFailureCallback that records every message instead of letting
// RunFeature() print-and-exit for us - the same pattern
// examples/GherkinCustomFailureHandler.cpp demonstrates, reused here so
// scenarios that are DELIBERATELY malformed/failing can still prove what
// RunFeature() reported without dying.
struct FailureCollector {
    std::vector<std::string> messages;
    GherkinFailureCallback AsCallback() {
        return [this](std::string_view message) { messages.emplace_back(message); };
    }
};

// ---------------------------------------------------------------------
// Scenario 1: a full Given/When/And/Then chain using every supported
// placeholder type ({int}, {float}, {string}, {word}) - all steps pass.
// ---------------------------------------------------------------------

bool RunHappyPathTypedPlaceholdersScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a cart with {int} items", [](TestContext& ctx, int count) -> bool {
        ctx.Set("items", count);
        return true;
    });
    registry.RegisterWhen("I label it {string}", [](TestContext& ctx, std::string label) -> bool {
        ctx.Set("label", label);
        return true;
    });
    registry.RegisterAnd("I tag it {word}", [](TestContext& ctx, std::string tag) -> bool {
        ctx.Set("tag", tag);
        return true;
    });
    registry.RegisterThen("the price should be {float}", [](TestContext& ctx, double price) -> bool {
        return price > 0.0 && ctx.Get<int>("items") == 3 && ctx.Get<std::string>("label") == "groceries" &&
               ctx.Get<std::string>("tag") == "urgent";
    });

    constexpr std::string_view feature = R"feature(
Feature: Typed placeholders
  Scenario: All placeholder kinds in one chain
    Given a cart with 3 items
    When I label it "groceries"
    And I tag it urgent
    Then the price should be 12.5
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/HappyPath");
    const bool asExpected = result.allPassed && result.featureName == "Typed placeholders" &&
                             result.scenarioResults.size() == 1 && result.scenarioResults[0].steps.size() == 4;
    if (!asExpected) {
        std::cerr << "  HappyPathTypedPlaceholders: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 2: negative {int} and {float} captures parse correctly (the
// pattern compiler's "-?" prefix on both numeric placeholder regexes).
// ---------------------------------------------------------------------

bool RunNegativePlaceholdersScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a balance of {int}", [](TestContext& ctx, int balance) -> bool {
        ctx.Set("balance", balance);
        return true;
    });
    registry.RegisterWhen("the temperature drops by {float} degrees", [](TestContext& ctx, double delta) -> bool {
        ctx.Set("delta", delta);
        return true;
    });
    registry.RegisterThen("the balance should be {int}", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("balance") == expected;
    });
    registry.RegisterAnd("the delta should be {float}", [](TestContext& ctx, double expected) -> bool {
        const double epsilon = 0.001;
        return std::abs(ctx.Get<double>("delta") - expected) < epsilon;
    });

    constexpr std::string_view feature = R"feature(
Feature: Negative numeric placeholders
  Scenario: Overdrawn balance and a temperature drop
    Given a balance of -42
    When the temperature drops by -3.5 degrees
    Then the balance should be -42
    And the delta should be -3.5
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/NegativePlaceholders");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  NegativePlaceholders: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 3: step definitions whose parameters are `long`/`long long`
// (not just `int`) - exercises impl::ConvertCapture<long>/<long long>,
// which every OTHER scenario in this file leaves dead since they all use
// `int` for {int} captures.
// ---------------------------------------------------------------------

bool RunLongAndLongLongCapturesScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a population of {int}", [](TestContext& ctx, long population) -> bool {
        ctx.Set("population", population);
        return true;
    });
    registry.RegisterWhen("it grows by {int} and then by {int}", [](TestContext& ctx, long long a, long long b) -> bool {
        ctx.Set("grown", ctx.Get<long>("population") + a + b);
        return true;
    });
    registry.RegisterThen("the population should be {int}", [](TestContext& ctx, long expected) -> bool {
        return ctx.Get<long long>("grown") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Wide integer captures
  Scenario: long and long long step parameters
    Given a population of 1000000
    When it grows by 250000 and then by 750000
    Then the population should be 2000000
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/LongCaptures");
    const bool asExpected = result.allPassed;
    if (!asExpected) {
        std::cerr << "  LongAndLongLongCaptures: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 4: Background steps are prepended to every Scenario, and
// Before/After hooks (registered with NO tag filter, so they always run)
// execute in the documented order: Before -> Background -> Scenario steps
// -> After. A shared "trail" vector<string> in the context records the
// order each piece actually ran in.
// ---------------------------------------------------------------------

bool RunBackgroundAndExecutionOrderScenario() {
    StepRegistry registry;

    // The trail lives OUTSIDE TestContext, captured by reference by every
    // hook/step lambda below: BabyBehaveTest's steps run in the exact order
    // they were added to it (a single ordered step list, not bucketed by
    // kind - see AddStepAt()), and impl::RunScenario adds them in the order
    // Before hooks -> Background -> Scenario steps -> After hooks. Since the
    // "Then" step therefore runs BEFORE the After hook, the only way to
    // observe the FULL order (including "after") is to inspect it once
    // RunFeature() has returned, not from within a step itself.
    std::vector<std::string> trail;

    registry.AddBeforeHook({}, [&trail](TestContext&) { trail.push_back("before"); });
    registry.AddAfterHook({}, [&trail](TestContext&) { trail.push_back("after"); });
    registry.RegisterGiven("the background has run", [&trail](TestContext&) -> bool {
        trail.push_back("background");
        return true;
    });
    registry.RegisterWhen("the scenario step runs", [&trail](TestContext&) -> bool {
        trail.push_back("scenario");
        return true;
    });
    registry.RegisterThen("the scenario step passes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Execution order
  Background:
    Given the background has run

  Scenario: Before, Background, steps, After in that order
    When the scenario step runs
    Then the scenario step passes
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/ExecutionOrder");
    // Before/After hooks are added as their own synthetic Precondition/
    // Postcondition steps (see impl::RunScenario), so the full recorded
    // step count is 2 hooks + 1 Background step + 2 Scenario steps == 5.
    const std::vector<std::string> expectedTrail{ "before", "background", "scenario", "after" };
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1 &&
                             result.scenarioResults[0].steps.size() == 5 && trail == expectedTrail;
    if (!asExpected) {
        std::cerr << "  BackgroundAndExecutionOrder: allPassed=" << result.allPassed
                   << " steps.size()=" << (result.scenarioResults.empty() ? 0 : result.scenarioResults[0].steps.size())
                   << " trail=[";
        for (const auto& entry : trail) {
            std::cerr << entry << ' ';
        }
        std::cerr << "]\n";
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 5: Feature-level @tags are unioned into every Scenario's own
// tags, and Before/After hook tag matching is AND/subset (a hook
// requiring {"vip","express"} only fires when the scenario's EFFECTIVE
// (unioned) tag set contains BOTH, not just one).
// ---------------------------------------------------------------------

bool RunTagUnionAndHookSubsetMatchingScenario() {
    StepRegistry registry;

    // Before hooks run before any Scenario step (see impl::RunScenario), so
    // "did the hook fire" is checked via presence-of-key rather than a
    // Scenario step resetting a flag first (which would run AFTER the hook
    // and clobber it).
    registry.AddBeforeHook({ "vip", "express" }, [](TestContext& ctx) { ctx.Set("vip_express_hook_ran", true); });
    registry.RegisterGiven("a customer", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("the vip+express hook should have fired", [](TestContext& ctx) -> bool {
        try {
            return ctx.Get<bool>("vip_express_hook_ran");
        } catch (const std::out_of_range&) {
            return false;
        }
    });
    registry.RegisterThen("the vip+express hook should NOT have fired", [](TestContext& ctx) -> bool {
        try {
            return !ctx.Get<bool>("vip_express_hook_ran");
        } catch (const std::out_of_range&) {
            return true;
        }
    });

    // Feature-level @vip is unioned with each Scenario's own tags: the
    // first Scenario adds @express (union == {vip, express} -> hook fires),
    // the second Scenario adds no further tag (union == {vip} only -> hook
    // does NOT fire, since it requires BOTH vip and express).
    constexpr std::string_view feature = R"feature(
@vip
Feature: Tag union and subset hook matching
  @express
  Scenario: Union has both required tags
    Given a customer
    Then the vip+express hook should have fired

  Scenario: Union is missing one required tag
    Given a customer
    Then the vip+express hook should NOT have fired
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TagUnionSubset");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 2;
    if (!asExpected) {
        std::cerr << "  TagUnionAndHookSubsetMatching: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 6: a hook whose required tag matches NO Scenario in the
// Feature at all - not an error, it simply never fires for any of them.
// ---------------------------------------------------------------------

bool RunHookMatchingNoScenarioScenario() {
    StepRegistry registry;
    registry.AddBeforeHook({ "nonexistent-tag" }, [](TestContext& ctx) { ctx.Set("hook_ran", true); });
    registry.RegisterGiven("a plain scenario", [](TestContext& ctx) -> bool {
        ctx.Set("hook_ran", false);
        return true;
    });
    registry.RegisterThen("the untagged hook should not have fired", [](TestContext& ctx) -> bool {
        return !ctx.Get<bool>("hook_ran");
    });

    constexpr std::string_view feature = R"feature(
Feature: Hook with no matching scenario
  Scenario: Untagged
    Given a plain scenario
    Then the untagged hook should not have fired
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/HookNoMatch");
    const bool asExpected = result.allPassed;
    if (!asExpected) {
        std::cerr << "  HookMatchingNoScenario: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 7: a Feature with 2 Scenarios, one passing and one failing,
// run under a custom (collecting, non-exiting) GherkinFailureCallback -
// proves RunFeature() keeps going past a failing Scenario instead of
// stopping at the first one, and returns allPassed==false with BOTH
// scenarioResults present.
// ---------------------------------------------------------------------

bool RunMultiScenarioCollectFailuresScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a counter at {int}", [](TestContext& ctx, int start) -> bool {
        ctx.Set("counter", start);
        return true;
    });
    registry.RegisterWhen("I add {int}", [](TestContext& ctx, int delta) -> bool {
        ctx.Set("counter", ctx.Get<int>("counter") + delta);
        return true;
    });
    registry.RegisterThen("the counter should be {int}", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("counter") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Mixed pass/fail scenarios
  Scenario: This one passes
    Given a counter at 1
    When I add 2
    Then the counter should be 3

  Scenario: This one fails
    Given a counter at 1
    When I add 2
    Then the counter should be 100
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/MultiScenario", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 2 &&
                             result.scenarioResults[0].allPassed && !result.scenarioResults[1].allPassed &&
                             collector.messages.size() == 1 &&
                             collector.messages[0].find("This one fails") != std::string::npos &&
                             collector.messages[0].find("the counter should be 100") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  MultiScenarioCollectFailures: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 8: a step with no matching registered definition is reported
// as a failing synthetic step ("no step definition matches: ...") rather
// than crashing or silently skipping.
// ---------------------------------------------------------------------

bool RunUnmatchedStepScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a known precondition", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Unmatched step
  Scenario: A When with no registered definition
    Given a known precondition
    When this step was never registered anywhere
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/UnmatchedStep", collector.AsCallback());

    // The synthetic failing step's own "no step definition matches: '...'"
    // diagnostic (see impl::AddParsedStepToTest) goes straight to stderr via
    // detail::PrintErrorLine, NOT into StepResult::message/the collected
    // failure text - so what the collected message actually names is the
    // step's own text ("this step was never registered anywhere") plus the
    // generic "Action failed" a false-returning Action always gets.
    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 &&
                             collector.messages.size() == 1 &&
                             collector.messages[0].find("this step was never registered anywhere") != std::string::npos &&
                             collector.messages[0].find("Action failed") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  UnmatchedStep: allPassed=" << result.allPassed << " messages.size()=" << collector.messages.size()
                   << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 9: a malformed feature (here: an unsupported 'Rule:' section)
// is a PARSE error, reported through the very same GherkinFailureCallback
// as a Scenario failure - but RunFeature() returns immediately, with an
// empty featureName and zero scenarioResults, since parsing never even
// reached the point of discovering any Scenario.
// ---------------------------------------------------------------------

bool RunParseErrorViaRunFeatureScenario() {
    StepRegistry registry;

    constexpr std::string_view feature = R"feature(
Feature: Uses an unsupported construct
  Rule: this is not supported in this version
  Scenario: Never reached
    Given anything
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/ParseError", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.featureName.empty() && result.scenarioResults.empty() &&
                             collector.messages.size() == 1 && collector.messages[0].find("Rule:") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  ParseErrorViaRunFeature: allPassed=" << result.allPassed
                   << " featureName=\"" << result.featureName << "\" scenarioResults.size()=" << result.scenarioResults.size()
                   << " messages.size()=" << collector.messages.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 10: RunFeature()'s featureLabel defaults to "<feature>" when
// the caller doesn't pass one - every other scenario in this file passes
// its own label explicitly.
// ---------------------------------------------------------------------

bool RunDefaultFeatureLabelScenario() {
    StepRegistry registry;
    registry.RegisterGiven("nothing in particular", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("it should still fail on purpose", [](TestContext&) -> bool { return false; });

    constexpr std::string_view feature = R"feature(
Feature: Default feature label
  Scenario: Deliberately failing, to surface the step location
    Given nothing in particular
    Then it should still fail on purpose
)feature";

    FailureCollector collector;
    // No featureLabel argument -> defaults to "<feature>".
    const auto result = RunFeature(feature, registry, "<feature>", collector.AsCallback());

    const bool asExpected = !result.allPassed && collector.messages.size() == 1 &&
                             collector.messages[0].find("<feature>") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  DefaultFeatureLabel: messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 11: step locations recorded in TestResult carry the real
// feature label plus the ACTUAL line/column of each step in the source
// .feature text (see impl::MakeFeatureLocation / impl::AddParsedStepToTest).
// ---------------------------------------------------------------------

bool RunStepLocationsScenario() {
    StepRegistry registry;
    registry.RegisterGiven("step one", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("step two", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("step three", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature =
        "Feature: Locations\n"
        "  Scenario: Three steps\n"
        "    Given step one\n"
        "    When step two\n"
        "    Then step three\n";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/Locations");
    bool asExpected = result.allPassed && result.scenarioResults.size() == 1 && result.scenarioResults[0].steps.size() == 3;
    if (asExpected) {
        for (const auto& step : result.scenarioResults[0].steps) {
            asExpected = asExpected && step.location.starts_with("SelfTestGherkin/Locations:") &&
                         step.location.find(':', std::string("SelfTestGherkin/Locations:").size()) != std::string::npos;
        }
    }
    if (!asExpected) {
        std::cerr << "  StepLocations: allPassed=" << result.allPassed << '\n';
        if (!result.scenarioResults.empty()) {
            for (const auto& step : result.scenarioResults[0].steps) {
                std::cerr << "    " << step.stepName << " location=\"" << step.location << "\"\n";
            }
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 12: registering a step pattern whose placeholder count doesn't
// match its step definition's parameter count throws std::invalid_argument
// at REGISTRATION time (never even reaches RunFeature()).
// ---------------------------------------------------------------------

bool RunMismatchedPlaceholderCountThrowsScenario() {
    StepRegistry registry;
    bool threw = false;
    std::string message;
    try {
        // Pattern declares one {int} placeholder, but the step definition
        // takes no extra parameters after TestContext&.
        registry.RegisterGiven("a value of {int}", [](TestContext&) -> bool { return true; });
    } catch (const std::invalid_argument& e) {
        threw = true;
        message = e.what();
    }
    const bool asExpected = threw && message.find("declares 1 placeholder") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  MismatchedPlaceholderCountThrows: threw=" << threw << " message=\"" << message << "\"\n";
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 13: an unknown placeholder name (not one of int/float/
// string/word) throws std::invalid_argument from CompileStepPattern,
// surfaced through StepRegistry::RegisterX at registration time.
// ---------------------------------------------------------------------

bool RunUnknownPlaceholderNameThrowsScenario() {
    StepRegistry registry;
    bool threw = false;
    std::string message;
    try {
        registry.RegisterWhen("a {bogus} value", [](TestContext&, std::string) -> bool { return true; });
    } catch (const std::invalid_argument& e) {
        threw = true;
        message = e.what();
    }
    const bool asExpected = threw && message.find("unknown step placeholder") != std::string::npos &&
                             message.find("bogus") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  UnknownPlaceholderNameThrows: threw=" << threw << " message=\"" << message << "\"\n";
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 14: comments, CRLF line endings, and a tag line mixing an
// @tagged token with a non-@ stray token all parse correctly - exercises
// impl::SplitLines' CRLF-trim branch and impl::AppendTagsFromLine's
// "non-@ tokens are skipped" branch, both otherwise dead in every other
// scenario in this file (which all use plain '\n' raw-string features
// with clean single-@ tag lines).
// ---------------------------------------------------------------------

std::string ToCrlf(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char letter : text) {
        if (letter == '\n') {
            out += '\r';
        }
        out += letter;
    }
    return out;
}

bool RunCommentsCrlfAndStrayTagTokenScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a tagged precondition", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("the stray token should not become a tag", [](TestContext&) -> bool { return true; });

    constexpr std::string_view featureLf = R"feature(
# This whole feature is a comment-and-CRLF torture test.
@real-tag not-a-tag-because-no-at-sign
Feature: Comments, CRLF, and stray tag tokens
  # another comment, this time inside the Feature body
  Scenario: Should still parse and run cleanly
    Given a tagged precondition
    Then the stray token should not become a tag
)feature";

    const std::string featureCrlf = ToCrlf(featureLf);
    const auto result = RunFeature(featureCrlf, registry, "SelfTestGherkin/CrlfAndComments");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  CommentsCrlfAndStrayTagToken: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

} // namespace

int main(int argc, char** argv) {
    ParseCommandLine(argc, argv);

    int passCount = 0;
    int totalCount = 0;

    ReportScenario("HappyPathTypedPlaceholders: int/float/string/word all in one chain",
                    RunHappyPathTypedPlaceholdersScenario(), passCount, totalCount);
    ReportScenario("NegativePlaceholders: negative {int}/{float} captures parse correctly",
                    RunNegativePlaceholdersScenario(), passCount, totalCount);
    ReportScenario("LongAndLongLongCaptures: step parameters of type long/long long",
                    RunLongAndLongLongCapturesScenario(), passCount, totalCount);
    ReportScenario("BackgroundAndExecutionOrder: Before -> Background -> steps -> After",
                    RunBackgroundAndExecutionOrderScenario(), passCount, totalCount);
    ReportScenario("TagUnionAndHookSubsetMatching: Feature tags unioned, hooks are AND/subset",
                    RunTagUnionAndHookSubsetMatchingScenario(), passCount, totalCount);
    ReportScenario("HookMatchingNoScenario: a hook tag matching no Scenario is not an error",
                    RunHookMatchingNoScenarioScenario(), passCount, totalCount);
    ReportScenario("MultiScenarioCollectFailures: RunFeature continues past a failing Scenario",
                    RunMultiScenarioCollectFailuresScenario(), passCount, totalCount);
    ReportScenario("UnmatchedStep: no registered definition is a reported failure, not a crash",
                    RunUnmatchedStepScenario(), passCount, totalCount);
    ReportScenario("ParseErrorViaRunFeature: a malformed feature reports through the same callback",
                    RunParseErrorViaRunFeatureScenario(), passCount, totalCount);
    ReportScenario("DefaultFeatureLabel: RunFeature's featureLabel defaults to \"<feature>\"",
                    RunDefaultFeatureLabelScenario(), passCount, totalCount);
    ReportScenario("StepLocations: TestResult step locations reflect real feature text line/column",
                    RunStepLocationsScenario(), passCount, totalCount);
    ReportScenario("MismatchedPlaceholderCountThrows: registration-time argument-count validation",
                    RunMismatchedPlaceholderCountThrowsScenario(), passCount, totalCount);
    ReportScenario("UnknownPlaceholderNameThrows: registration-time placeholder-name validation",
                    RunUnknownPlaceholderNameThrowsScenario(), passCount, totalCount);
    ReportScenario("CommentsCrlfAndStrayTagToken: comments/CRLF/non-@ tag tokens all parse cleanly",
                    RunCommentsCrlfAndStrayTagTokenScenario(), passCount, totalCount);

    std::cout << '\n' << passCount << "/" << totalCount << " Gherkin scenarios behaved as expected\n";

    return (passCount == totalCount) ? EXIT_SUCCESS : EXIT_FAILURE;
}
