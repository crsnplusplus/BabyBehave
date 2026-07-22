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

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Explicit alias for the internal parsing/matching namespace (mirrors
// test_Gherkin_Parser.cpp's own GherkinImpl alias convention) - used below
// by RunInternalDefensiveGuardsScenario, which calls straight into
// impl:: functions to exercise a handful of "unreachable in practice"
// defensive branches (see that scenario's own doc comment).
namespace GherkinImpl = BabyBehave::BDD::Gherkin::impl;

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
    // kind - see AddStepAt()), and impl::RunScenarioAttempt adds them in the order
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
    // Postcondition steps (see impl::RunScenarioAttempt), so the full recorded
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

    // Before hooks run before any Scenario step (see impl::RunScenarioAttempt), so
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

// ---------------------------------------------------------------------
// Scenario 15: a Scenario Outline with an Examples: table expands into
// one independent Scenario per data row (see impl::ExpandScenarioOutlines) -
// each row's <placeholder> tokens are substituted into every step's text
// before normal step matching runs, and all resulting scenarios pass.
// ---------------------------------------------------------------------

bool RunScenarioOutlineExpansionScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a shelf holds {int} items", [](TestContext& ctx, int start) -> bool {
        ctx.Set("shelf", start);
        return true;
    });
    registry.RegisterWhen("I restock it with {int} more {word}", [](TestContext& ctx, int add, std::string item) -> bool {
        ctx.Set("shelf", ctx.Get<int>("shelf") + add);
        ctx.Set("item", item);
        return true;
    });
    registry.RegisterThen("the shelf should hold {int} items", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("shelf") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Restock a shelf
  Scenario Outline: Restocking adds to the current count
    Given a shelf holds <start> items
    When I restock it with <add> more <item>
    Then the shelf should hold <total> items

    Examples:
      | start | add | item   | total |
      | 2     | 8   | plums  | 10    |
      | 0     | 4   | grapes | 4     |
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/OutlineExpansion");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 2 &&
                             result.scenarioResults[0].testName.find("Examples row 1") != std::string::npos &&
                             result.scenarioResults[1].testName.find("Examples row 2") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  ScenarioOutlineExpansion: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 16: five distinct Scenario Outline/Examples malformed-syntax
// parse errors, each independently rejected with zero scenarios executed
// (see impl::FinalizeCurrentScenarioExamples and the '|' row handling in
// impl::ProcessFeatureLine for the actual 5 conditions this parser
// enforces - a <placeholder> with no matching Examples column is
// deliberately NOT one of them: SubstitutePlaceholders leaves it as
// literal text instead of erroring, see its own doc comment).
// ---------------------------------------------------------------------

bool RunScenarioOutlineMalformedScenario() {
    struct Case {
        std::string_view description;
        std::string_view feature;
        std::string_view expectedSubstring;
    };
    const std::array<Case, 5> cases{ {
        // 1. 'Scenario Outline:' with no 'Examples:'/'Scenarios:' table at all.
        { "Outline with no Examples table", R"feature(
Feature: Outline missing Examples
  Scenario Outline: No Examples table follows
    Given a value of <x>
)feature",
          "has no 'Examples:'/'Scenarios:' table" },
        // 2. An Examples row whose cell count disagrees with the header.
        { "Examples row with wrong cell count", R"feature(
Feature: Outline with a mismatched row
  Scenario Outline: Row width disagrees with header
    Given a value of <x>

    Examples:
      | x |
      | 1 | 2 |
)feature",
          "cell(s) but the header declares" },
        // 3. A <placeholder> is fine even with no matching column (left
        //    literal - NOT an error); what IS an error is 'Examples:'
        //    appearing without a preceding 'Scenario Outline:'/'Scenario
        //    Template:'.
        { "Examples table without a preceding Outline", R"feature(
Feature: Examples without an Outline
  Scenario: An ordinary Scenario, not an Outline
    Given a value of <x>

    Examples:
      | x |
      | 1 |
)feature",
          "without a preceding 'Scenario Outline:'/'Scenario Template:'" },
        // 4. An Examples table with only a header row and zero data rows.
        { "Examples table with zero data rows", R"feature(
Feature: Outline with an empty Examples table
  Scenario Outline: Header only, no data rows
    Given a value of <x>

    Examples:
      | x |
)feature",
          "must have at least one data row" },
        // 5. Regression test: a step keyword reached after this Outline's
        //    own (already-closed) Examples: table used to be silently
        //    reattached to the Outline scenario, with any trailing '|'
        //    lines after it misread as a fresh Examples row/Data Table -
        //    now a clear parse error instead.
        { "Step keyword after this Outline's own Examples table", R"feature(
Feature: Step after examples
  Scenario Outline: X
    Given a value of <x>

  Examples:
    | x |
    | 1 |
    Given something weird after examples
      | a |
      | zzz |
)feature",
          "step keyword found after" },
    } };

    bool asExpected = true;
    for (const auto& testCase : cases) {
        StepRegistry registry; // Never reached: every case fails at parse time.
        FailureCollector collector;
        const auto result = RunFeature(testCase.feature, registry, "SelfTestGherkin/OutlineMalformed", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && result.featureName.empty() && result.scenarioResults.empty() &&
                                     collector.messages.size() == 1 &&
                                     collector.messages[0].find(testCase.expectedSubstring) != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  ScenarioOutlineMalformed[" << testCase.description << "]: allPassed=" << result.allPassed
                       << " messages.size()=" << collector.messages.size() << '\n';
            for (const auto& message : collector.messages) {
                std::cerr << "    " << message << '\n';
            }
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 16b: regression test - a Scenario Outline with TWO separate
// Examples:/Scenarios: blocks (rather than the single block every other
// Outline scenario in this file uses) MERGES every row from both blocks,
// in declaration order, into the same expansion - real Cucumber supports
// multiple named Examples: blocks per outline. Before this was fixed, only
// the LAST Examples: block's rows survived (see
// impl::HandleExamplesTableRow and the 'Examples:'/'Scenarios:' line
// handling in impl::ProcessFeatureLine).
// ---------------------------------------------------------------------

bool RunScenarioOutlineMultipleExamplesBlocksScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a shelf holds {int} items", [](TestContext& ctx, int start) -> bool {
        ctx.Set("shelf", start);
        return true;
    });

    constexpr std::string_view feature = R"feature(
Feature: Multiple Examples blocks
  Scenario Outline: Restocking starts at <start>

    Given a shelf holds <start> items

    Examples: First batch
      | start |
      | 1     |
      | 2     |

    Examples: Second batch
      | start |
      | 3     |
      | 4     |
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/OutlineMultipleExamples");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 4 &&
                             result.scenarioResults[0].testName.find("Examples row 1") != std::string::npos &&
                             result.scenarioResults[1].testName.find("Examples row 2") != std::string::npos &&
                             result.scenarioResults[2].testName.find("Examples row 3") != std::string::npos &&
                             result.scenarioResults[3].testName.find("Examples row 4") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  ScenarioOutlineMultipleExamples: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 17: a Scenario tagged with a generous @timeout that no step
// comes close to exceeding - the timeout annotation is parsed and its
// ScenarioDeadline is active, but Expired() never returns true, so the
// scenario simply passes as if untagged (see impl::WrapWithDeadlineCheck).
// ---------------------------------------------------------------------

bool RunTimeoutAnnotationNoTripScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a quick operation", [](TestContext&) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return true;
    });
    registry.RegisterThen("it should complete", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
@timeout:2s
Feature: Timeout does not trip
  Scenario: A quick step comfortably within budget
    Given a quick operation
    Then it should complete
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TimeoutNoTrip");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  TimeoutAnnotationNoTrip: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 18: a Scenario tagged with a TIGHT @timeout where a step
// sleeps past the deadline - impl::WrapWithDeadlineCheck's cooperative,
// INTER-STEP check only fires BEFORE a step runs, so the sleeping step
// itself always completes; it is the NEXT step's precheck that finds the
// deadline already expired and throws, reported as a failed Scenario
// with a timeout-related message via a collecting (non-exiting) callback.
// ---------------------------------------------------------------------

bool RunTimeoutAnnotationTripScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a slow operation", [](TestContext&) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return true;
    });
    registry.RegisterThen("it should never be reached", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
@timeout:100ms
Feature: Timeout trips mid-scenario
  Scenario: A step blocks past the deadline
    Given a slow operation
    Then it should never be reached
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TimeoutTrip", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 &&
                             !result.scenarioResults[0].allPassed && collector.messages.size() == 1 &&
                             collector.messages[0].find("exceeded @timeout deadline") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  TimeoutAnnotationTrip: allPassed=" << result.allPassed
                   << " messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 19: a bare, unitless @timeout value (no s/ms/m suffix) is
// rejected as a malformed annotation BEFORE any Before hook/Background/
// step ever runs (see impl::ParseTimeoutValue - a unit suffix is
// mandatory) - the synthetic parse-error StepResult is the only step
// recorded, and the step that would otherwise run is never invoked.
// ---------------------------------------------------------------------

bool RunTimeoutAnnotationMalformedScenario() {
    StepRegistry registry;
    int invocationCount = 0;
    registry.RegisterGiven("a step that should never run", [&invocationCount](TestContext&) -> bool {
        ++invocationCount;
        return true;
    });

    constexpr std::string_view feature = R"feature(
@timeout:5
Feature: Malformed timeout value
  Scenario: A bare unitless timeout value is rejected
    Given a step that should never run
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TimeoutMalformed", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 &&
                             result.scenarioResults[0].steps.size() == 1 && invocationCount == 0 &&
                             collector.messages.size() == 1 &&
                             collector.messages[0].find("malformed @timeout value") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  TimeoutAnnotationMalformed: allPassed=" << result.allPassed
                   << " invocationCount=" << invocationCount << " messages.size()=" << collector.messages.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 20: 3 independent Scenarios run under enableParallelScenarios
// == true (RunFeature's 5th parameter), each dispatched to its own
// std::async task and sleeping for a DIFFERENT duration chosen so their
// completion order is the reverse of their declaration order - proving
// result.scenarioResults comes back in original declaration/index order
// (see RunFeature's preallocated-by-index scenarioResults comment), not
// completion order.
// ---------------------------------------------------------------------

bool RunParallelScenariosInOriginalOrderScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a counter at {int}", [](TestContext& ctx, int start) -> bool {
        ctx.Set("counter", start);
        return true;
    });
    registry.RegisterWhen("I wait {int} ms and add {int}", [](TestContext& ctx, int waitMs, int add) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        ctx.Set("counter", ctx.Get<int>("counter") + add);
        return true;
    });
    registry.RegisterThen("the counter should be {int}", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("counter") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Parallel scenarios preserve declaration order
  Scenario: First scenario, slowest to finish
    Given a counter at 1
    When I wait 150 ms and add 10
    Then the counter should be 11

  Scenario: Second scenario, finishes in the middle
    Given a counter at 2
    When I wait 100 ms and add 20
    Then the counter should be 22

  Scenario: Third scenario, fastest to finish
    Given a counter at 3
    When I wait 50 ms and add 30
    Then the counter should be 33
)feature";

    FailureCollector collector;
    const auto result =
        RunFeature(feature, registry, "SelfTestGherkin/ParallelOrder", collector.AsCallback(), /*enableParallelScenarios=*/true);

    const bool asExpected = result.allPassed && result.scenarioResults.size() == 3 &&
                             result.scenarioResults[0].testName.find("First scenario") != std::string::npos &&
                             result.scenarioResults[1].testName.find("Second scenario") != std::string::npos &&
                             result.scenarioResults[2].testName.find("Third scenario") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  ParallelScenariosInOriginalOrder: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size() << '\n';
        for (const auto& scenarioResult : result.scenarioResults) {
            std::cerr << "    " << scenarioResult.testName << '\n';
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 21: a full Given/When/And/But step-keyword chain - But is a
// distinct registration method (StepRegistry::RegisterBut), matched and
// executed exactly like Given/When/Then/And, not an alias for any of
// them (see impl::MatchStepKeyword's "But " prefix and
// impl::AddParsedStepToTest's StepKeyword::But case).
// ---------------------------------------------------------------------

bool RunButStepKeywordScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a cart with {int} item", [](TestContext& ctx, int count) -> bool {
        ctx.Set("cart", count);
        return true;
    });
    registry.RegisterWhen("I add another item", [](TestContext& ctx) -> bool {
        ctx.Set("cart", ctx.Get<int>("cart") + 1);
        return true;
    });
    registry.RegisterAnd("it now has {int} items", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("cart") == expected;
    });
    registry.RegisterBut("it should not have {int} items", [](TestContext& ctx, int notExpected) -> bool {
        return ctx.Get<int>("cart") != notExpected;
    });

    constexpr std::string_view feature = R"feature(
Feature: But step keyword
  Scenario: Shopping cart Given/When/And/But chain
    Given a cart with 1 item
    When I add another item
    And it now has 2 items
    But it should not have 3 items
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/ButStepKeyword");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1 &&
                             result.scenarioResults[0].steps.size() == 4 &&
                             result.scenarioResults[0].steps.back().stepLabel == "But";
    if (!asExpected) {
        std::cerr << "  ButStepKeyword: allPassed=" << result.allPassed
                   << " steps.size()=" << (result.scenarioResults.empty() ? 0 : result.scenarioResults[0].steps.size())
                   << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 22: a Scenario tagged with BOTH a tag-matched Before hook
// (@vip) AND a generous, never-tripping @timeout - proves the Before
// hook itself runs correctly (and is deadline-checked, per
// RunScenarioAttempt wrapping every Before hook when a deadline is
// active) under an active-but-non-tripping timeout policy, and the
// scenario completes successfully.
// ---------------------------------------------------------------------

bool RunBeforeHookWithActiveTimeoutScenario() {
    StepRegistry registry;
    registry.AddBeforeHook({ "vip" }, [](TestContext& ctx) { ctx.Set("vip_hook_ran", true); });
    registry.RegisterGiven("a customer profile", [](TestContext&) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return true;
    });
    registry.RegisterThen("the vip hook should have fired", [](TestContext& ctx) -> bool {
        try {
            return ctx.Get<bool>("vip_hook_ran");
        } catch (const std::out_of_range&) {
            return false;
        }
    });

    constexpr std::string_view feature = R"feature(
@vip @timeout:5s
Feature: Before hook under an active, never-tripping timeout
  Scenario: Vip customer, generous deadline
    Given a customer profile
    Then the vip hook should have fired
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/BeforeHookWithTimeout");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  BeforeHookWithActiveTimeout: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 23: a step definition taking (TestContext&, const DataTable&)
// alone (no placeholder captures) - StepDefinitionRawArgKind's N ==
// placeholderCount + 1 branch with placeholderCount == 0.
// ---------------------------------------------------------------------

bool RunDataTableBasicScenario() {
    StepRegistry registry;
    registry.RegisterGiven("the following expenses were recorded:", [](TestContext& ctx, const DataTable& table) -> bool {
        double total = 0.0;
        for (std::size_t i = 0; i < table.RowCount(); ++i) {
            total += std::stod(table.Get(i, "amount"));
        }
        ctx.Set("total", total);
        return true;
    });
    registry.RegisterThen("the total expense should be {float}", [](TestContext& ctx, double expected) -> bool {
        const double epsilon = 0.001;
        return std::abs(ctx.Get<double>("total") - expected) < epsilon;
    });

    constexpr std::string_view feature = R"feature(
Feature: Data table with no captures
  Scenario: Sum an expenses table with a table-only step definition
    Given the following expenses were recorded:
      | item     | amount |
      | coffee   | 12.50  |
      | notebook | 8.75   |
      | pen      | 29.00  |
    Then the total expense should be 50.25
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DataTableBasic");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  DataTableBasic: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 24: a step definition taking a {word} placeholder capture
// PLUS a trailing const DataTable& parameter together, proving capture +
// raw-argument composition (StepDefinitionRawArgKind's N ==
// placeholderCount + 1 branch with placeholderCount == 1).
// ---------------------------------------------------------------------

bool RunDataTableWithCaptureScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a {word} database with users:", [](TestContext& ctx, std::string env, const DataTable& table) -> bool {
        ctx.Set("env", env);
        ctx.Set("user_count", static_cast<int>(table.RowCount()));
        ctx.Set("first_user", table.Get(0, "name"));
        return true;
    });
    registry.RegisterThen("the environment should be {word}", [](TestContext& ctx, std::string expected) -> bool {
        return ctx.Get<std::string>("env") == expected;
    });
    registry.RegisterAnd("there should be {int} users", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("user_count") == expected;
    });
    registry.RegisterAnd("the first user should be {word}", [](TestContext& ctx, std::string expected) -> bool {
        return ctx.Get<std::string>("first_user") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Data table with a placeholder capture
  Scenario: Word capture plus a trailing data table
    Given a staging database with users:
      | name  | role   |
      | alice | admin  |
      | bob   | editor |
    Then the environment should be staging
    And there should be 2 users
    And the first user should be alice
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DataTableWithCapture");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  DataTableWithCapture: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 25: a step deliberately calls DataTable::Get(row, "unknown
// column"), letting the resulting std::invalid_argument propagate up
// through executeStep's exception handling (forced collect-failures
// mode - see RunScenarioAttempt), reported as a failed Scenario with a
// message naming the unknown column.
// ---------------------------------------------------------------------

bool RunDataTableGetUnknownColumnThrowsScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a shipment manifest:", [](TestContext&, const DataTable& table) -> bool {
        // Deliberately wrong column name: exercises DataTable::Get's throw path.
        (void)table.Get(0, "nonexistent_column");
        return true;
    });

    constexpr std::string_view feature = R"feature(
Feature: Data table unknown column
  Scenario: A step accesses a data table column that does not exist
    Given a shipment manifest:
      | sku | quantity |
      | A1  | 5        |
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DataTableUnknownColumn", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 &&
                             collector.messages.size() == 1 &&
                             collector.messages[0].find("nonexistent_column") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  DataTableGetUnknownColumnThrows: allPassed=" << result.allPassed
                   << " messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 26: a step definition taking (TestContext&, const
// std::string&) alone, matched against a step followed by a multi-line
// Doc String that includes a blank line (proving blank-line preservation)
// and a more-indented line (proving StripDocStringIndent's per-line,
// up-to-the-opening-column stripping, not a fixed strip).
// ---------------------------------------------------------------------

bool RunDocStringBasicScenario() {
    const std::string expected = "Line one at base indent.\n"
                                  "\n"
                                  "    Line two indented further.\n"
                                  "Line three back at base indent.";

    StepRegistry registry;
    registry.RegisterGiven("the following specification:", [expected](TestContext& ctx, const std::string& spec) -> bool {
        ctx.Set("spec_matches", spec == expected);
        return true;
    });
    registry.RegisterThen("the specification should match exactly", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("spec_matches");
    });

    constexpr std::string_view feature = "Feature: Doc string basic\n"
                                          "  Scenario: Multi-line spec with a blank line and mixed indentation\n"
                                          "    Given the following specification:\n"
                                          "      \"\"\"\n"
                                          "      Line one at base indent.\n"
                                          "\n"
                                          "          Line two indented further.\n"
                                          "      Line three back at base indent.\n"
                                          "      \"\"\"\n"
                                          "    Then the specification should match exactly\n";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DocStringBasic");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  DocStringBasic: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 27: a step definition taking a {word} placeholder capture
// PLUS a trailing Doc String together, proving capture + Doc-String
// raw-argument composition (mirrors Scenario 24's Data Table analogue).
// ---------------------------------------------------------------------

bool RunDocStringWithCaptureScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a {word} with the following specification:", [](TestContext& ctx, std::string item, const std::string& spec) -> bool {
        ctx.Set("item", item);
        ctx.Set("spec", spec);
        return true;
    });
    registry.RegisterThen("the item should be {word}", [](TestContext& ctx, std::string expected) -> bool {
        return ctx.Get<std::string>("item") == expected;
    });
    registry.RegisterAnd("the specification should mention {word}", [](TestContext& ctx, std::string term) -> bool {
        return ctx.Get<std::string>("spec").find(term) != std::string::npos;
    });

    constexpr std::string_view feature = R"feature(
Feature: Doc string with a placeholder capture
  Scenario: Word capture plus a trailing doc string
    Given a widget with the following specification:
      """
      A small mechanical widget.
      Weighs under one kilogram.
      """
    Then the item should be widget
    And the specification should mention mechanical
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DocStringWithCapture");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  DocStringWithCapture: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 28: a Scenario tagged with @retry:3 where a step fails on its
// first 1-2 invocations (tracked via a static counter captured by the step
// lambda) and succeeds on a later invocation. Proves that RetryAnnotation
// genuinely re-attempts the scenario: the scenario is ultimately reported
// as PASSED (allPassed==true), demonstrating that the retry mechanism worked
// and the scenario recovered from transient failure. This tests the
// ParseRetryValue, RunScenarioAttempt, and RunScenarioWithRetries code paths
// that otherwise remain untested in self-hosted scenarios.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunRetryScenarioSucceedsAfterInitialFailureScenario() {
    StepRegistry registry;

    // A mutable counter, captured by reference by the step lambda below.
    // It tracks how many times this particular step is invoked across the
    // multiple retry attempts. Deliberately an ordinary (non-static) local:
    // it only needs to live as long as this function call (the lambda runs
    // synchronously inside RunFeature() below, before this function
    // returns), and a `static` here would be both unnecessary and would
    // trigger a "capture of variable with non-automatic storage duration"
    // compiler warning for no benefit.
    int flaky_step_invocation_count = 0;

    // A step that fails on invocations 1-2, but succeeds on invocation 3.
    // Each retry attempt is a complete re-run of Before hooks + Background +
    // Scenario steps + After hooks, so this counter increments once per
    // attempt, not per successful retry.
    registry.RegisterGiven("a flaky operation", [&flaky_step_invocation_count](TestContext& ctx) -> bool {
        ++flaky_step_invocation_count;
        ctx.Set("invocation_count", flaky_step_invocation_count);
        // Fail on the first two invocations, succeed on the third.
        return flaky_step_invocation_count >= 3;
    });

    registry.RegisterThen("it should eventually succeed", [](TestContext& ctx) -> bool {
        // By the time this step runs in a passing attempt, the flaky operation
        // has succeeded (returned true), so this assertion simply confirms we
        // reached it and the invocation count is as expected.
        return ctx.Get<int>("invocation_count") >= 3;
    });

    constexpr std::string_view feature = R"feature(
@retry:3
Feature: Retry annotation recovers from transient failure
  Scenario: A flaky step fails twice, succeeds on third attempt
    Given a flaky operation
    Then it should eventually succeed
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/RetrySucceedsAfterFailure");

    // Verify:
    // 1. allPassed == true (the scenario ultimately passed after retries)
    // 2. scenarioResults.size() == 1 (exactly one scenario in the feature)
    // 3. The final reported step count is exactly 2 (one Given, one Then) -
    //    only the FINAL (successful) attempt is reported, not intermediate
    //    failed attempts.
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1 &&
                             result.scenarioResults[0].steps.size() == 2;
    if (!asExpected) {
        std::cerr << "  RetryScenarioSucceedsAfterInitialFailure: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " steps.size()=" << (result.scenarioResults.empty() ? 0 : result.scenarioResults[0].steps.size())
                   << " final_invocation_count=" << flaky_step_invocation_count << '\n';
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 29: a Scenario tagged with @retry:2 where EVERY attempt fails -
// confirms the final failure is reported EXACTLY ONCE to onFailure (via
// FailureCollector), and allPassed == false. This tests the code path
// where the loop in RunScenarioWithRetries exhausts all maxAttempts and
// falls through to report the truly-final outcome.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunRetryScenarioAllAttemptsFailScenario() {
    StepRegistry registry;

    // A step that always fails, no matter how many times it's invoked.
    registry.RegisterGiven("an operation that always fails", [](TestContext&) -> bool {
        return false;
    });

    registry.RegisterThen("it should never succeed", [](TestContext&) -> bool {
        return true; // This is never reached because the Given step fails first
    });

    constexpr std::string_view feature = R"feature(
@retry:2
Feature: Retry annotation with persistent failure
  Scenario: All retry attempts fail, reported exactly once
    Given an operation that always fails
    Then it should never succeed
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/RetryAllAttemptsFail", collector.AsCallback());

    // Verify:
    // 1. allPassed == false (the scenario ultimately failed)
    // 2. scenarioResults.size() == 1 (exactly one scenario)
    // 3. The scenario's allPassed is also false
    // 4. onFailure was invoked EXACTLY ONCE (FailureCollector has exactly one message)
    //    - this proves intermediate failed attempts were NOT reported, only the
    //    - final outcome (see RunScenarioWithRetries: "onFailure is invoked ONLY
    //    - when attempt == maxAttempts")
    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 &&
                             !result.scenarioResults[0].allPassed &&
                             collector.messages.size() == 1;
    if (!asExpected) {
        std::cerr << "  RetryScenarioAllAttemptsFail: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " scenario[0].allPassed=" << (result.scenarioResults.empty() ? false : result.scenarioResults[0].allPassed)
                   << " messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 30: an expression-based Before hook (StepRegistry::AddBeforeHookExpr,
// Feature 7: AND/OR/NOT tag expressions) registered with "@smoke and not
// @slow" against a Feature with 3 Scenarios carrying different tag
// combinations - proves the hook fires ONLY for the one Scenario whose
// EFFECTIVE tags satisfy the full AND+NOT expression (@smoke present AND
// @slow absent), not for a Scenario carrying BOTH tags nor for one
// carrying neither the required tag. An external counter (captured by
// reference, outside TestContext - same rationale as Scenario 4's
// "trail") proves the TOTAL fire count across all 3 Scenarios is exactly
// 1, while a per-Scenario TestContext flag (same presence-of-key pattern
// as Scenario 5's vector-based hook test) proves WHICH Scenario it fired
// for. This exercises TokenizeTagExpression, ParseTagExpression,
// EvaluateTagExpression, and MatchesHookTags' expression-based dispatch
// branch end-to-end through the real RunFeature API - all otherwise
// covered only at the gtest/UT level (test_Gherkin_Parser.cpp,
// test_Gherkin_Integration.cpp), never yet through BBH's self-hosted
// RunFeature surface.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunTagExpressionHookAndNotScenario() {
    StepRegistry registry;

    // Lives outside TestContext (a fresh TestContext per Scenario would
    // hide a cross-Scenario total), captured by reference by the hook
    // lambda below - same rationale as Scenario 4's "trail".
    int smokeNotSlowHookFireCount = 0;

    registry.AddBeforeHookExpr("@smoke and not @slow", [&smokeNotSlowHookFireCount](TestContext& ctx) {
        ++smokeNotSlowHookFireCount;
        ctx.Set("smoke_not_slow_hook_ran", true);
    });
    registry.RegisterGiven("a customer", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("the smoke-not-slow hook should have fired", [](TestContext& ctx) -> bool {
        try {
            return ctx.Get<bool>("smoke_not_slow_hook_ran");
        } catch (const std::out_of_range&) {
            return false;
        }
    });
    registry.RegisterThen("the smoke-not-slow hook should NOT have fired", [](TestContext& ctx) -> bool {
        try {
            return !ctx.Get<bool>("smoke_not_slow_hook_ran");
        } catch (const std::out_of_range&) {
            return true;
        }
    });

    constexpr std::string_view feature = R"feature(
Feature: Expression-based Before hook (AND/NOT)
  @smoke
  Scenario: Smoke only - satisfies "@smoke and not @slow"
    Given a customer
    Then the smoke-not-slow hook should have fired

  @smoke @slow
  Scenario: Smoke and slow together - the NOT @slow term fails
    Given a customer
    Then the smoke-not-slow hook should NOT have fired

  @slow
  Scenario: Slow only, no smoke - the @smoke term fails
    Given a customer
    Then the smoke-not-slow hook should NOT have fired
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TagExpressionHookAndNot");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 3 && smokeNotSlowHookFireCount == 1;
    if (!asExpected) {
        std::cerr << "  TagExpressionHookAndNot: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " smokeNotSlowHookFireCount=" << smokeNotSlowHookFireCount << '\n';
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 31: a registry with BOTH a vector-based Before hook
// (AddBeforeHook, AND/subset matching) AND an expression-based Before
// hook (AddBeforeHookExpr, "@a or @b" - exercising the Or branch left
// uncovered by Scenario 30's And/Not-only expression) registered
// simultaneously - proves the two mechanisms coexist on the same
// registry without cross-interference: the vector-based hook only cares
// about @needs, the expression-based hook only cares about @a/@b, and a
// Scenario carrying tags relevant to BOTH fires BOTH independently.
// Mirrors the gtest-level
// GherkinIntegration.VectorBasedAndExpressionBasedHooksCoexistWithoutCrossInterference
// test in tests/test_Gherkin_Integration.cpp, but exercised through the
// real self-hosted RunFeature API instead of gtest assertions.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunVectorAndExpressionHooksCoexistScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    int vectorHookFireCount = 0;
    int exprHookFireCount = 0;

    // Vector-based (AND/subset): fires only when @needs is present; @a/@b
    // are irrelevant to it.
    registry.AddBeforeHook({ "needs" }, [&vectorHookFireCount](TestContext&) { ++vectorHookFireCount; });
    // Expression-based (Or): fires when @a or @b is present; @needs is
    // irrelevant to it.
    registry.AddBeforeHookExpr("@a or @b", [&exprHookFireCount](TestContext&) { ++exprHookFireCount; });

    constexpr std::string_view feature = R"feature(
Feature: Vector-based and expression-based hooks coexist
  @needs
  Scenario: A - only the tag the vector-based hook cares about
    Given a step

  @a
  Scenario: B - only a tag the expression-based hook cares about
    Given a step

  @needs @a
  Scenario: C - one tag from each mechanism - both hooks must fire
    Given a step

  Scenario: D - untagged - neither hook fires
    Given a step
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/VectorAndExpressionHooksCoexist");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 4 && vectorHookFireCount == 2 &&
                             exprHookFireCount == 2;
    if (!asExpected) {
        std::cerr << "  VectorAndExpressionHooksCoexist: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " vectorHookFireCount=" << vectorHookFireCount << " exprHookFireCount=" << exprHookFireCount
                   << '\n';
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 32: closes the two BBH-level gaps self-reported alongside
// Feature 7 (tag expressions): (1) parenthesized grouping "(...)" - a
// brand-new lexer/parser construct (LParen/RParen tokens,
// TagExpressionParser::ParsePrimary's paren branch) never exercised end-
// to-end through RunFeature by Scenarios 30/31 above (which only use
// "and"/"or"/"not" without parens); and (2) a malformed expression passed
// to AddBeforeHookExpr throwing immediately at registration time, mirroring
// RunMismatchedPlaceholderCountThrowsScenario/RunUnknownPlaceholderNameThrowsScenario's
// registration-time-throw style (applied here to tag expressions instead
// of step patterns). Both are already thoroughly covered at the UT/gtest
// level (GherkinTagExpression.ParenthesesOverrideDefaultPrecedence and the
// 5 malformed-expression-error tests in test_Gherkin_Parser.cpp); this
// closes the same gap at the BBH/self-hosted level for consistency with
// how Scenario 12 (ScenarioOutlineMalformed) and Scenario 15
// (TimeoutAnnotationMalformed) already do so for their own features.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunTagExpressionParensAndMalformedThrowScenario() {
    // Part 1: registration-time malformed-expression throw (unbalanced
    // parens) - the hook must never be registered.
    StepRegistry throwRegistry;
    bool threw = false;
    try {
        throwRegistry.AddBeforeHookExpr("(@a or @b", [](TestContext&) {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    const bool throwAsExpected = threw && throwRegistry.BeforeHooks().empty();
    if (!throwAsExpected) {
        std::cerr << "  TagExpressionParensAndMalformedThrow: malformed AddBeforeHookExpr did not throw as "
                     "expected (threw=" << threw << ")\n";
    }

    // Part 2: a real, valid parenthesized expression - "(@a or @b) and not
    // @slow" - drives a Before hook end-to-end through RunFeature.
    StepRegistry registry;
    int hookFireCount = 0;
    registry.AddBeforeHookExpr("(@a or @b) and not @slow", [&hookFireCount](TestContext& ctx) {
        ++hookFireCount;
        ctx.Set("parens_hook_ran", true);
    });
    registry.RegisterGiven("a customer", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("the parens hook should have fired", [](TestContext& ctx) -> bool {
        try {
            return ctx.Get<bool>("parens_hook_ran");
        } catch (const std::out_of_range&) {
            return false;
        }
    });
    registry.RegisterThen("the parens hook should NOT have fired", [](TestContext& ctx) -> bool {
        try {
            return !ctx.Get<bool>("parens_hook_ran");
        } catch (const std::out_of_range&) {
            return true;
        }
    });

    constexpr std::string_view feature = R"feature(
Feature: Parenthesized tag expression grouping
  @a
  Scenario: a only - satisfies "(@a or @b) and not @slow" via the left OR branch
    Given a customer
    Then the parens hook should have fired

  @a @slow
  Scenario: a and slow - the NOT @slow term fails
    Given a customer
    Then the parens hook should NOT have fired

  @b
  Scenario: b only - satisfies via the right OR branch inside the parens
    Given a customer
    Then the parens hook should have fired
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/TagExpressionParens");
    const bool parensAsExpected = result.allPassed && result.scenarioResults.size() == 3 && hookFireCount == 2;
    if (!parensAsExpected) {
        std::cerr << "  TagExpressionParensAndMalformedThrow: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " hookFireCount=" << hookFireCount << '\n';
    }

    return throwAsExpected && parensAsExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 33: Suite-level Before-ALL/After-ALL hooks (Feature 8:
// StepRegistry::AddBeforeAllHook/AddAfterAllHook) registered against a
// Feature with 3 Scenarios - proves both hooks fire EXACTLY ONCE across
// the whole Feature, not once per Scenario (unlike AddBeforeHook/
// AddAfterHook, which run once per Scenario - see Scenario 4). External
// counters (captured by reference, outside TestContext - same rationale
// as Scenario 4's "trail" and Scenario 30's "smokeNotSlowHookFireCount")
// prove the fire counts; SuiteHookFunction is void() (no TestContext&
// parameter), so there is no per-Scenario context to inspect from inside
// the hook itself. This exercises registry.AddBeforeAllHook/
// AddAfterAllHook and RunFeature's prologue loop end-to-end through the
// real RunFeature API for the first time in this file.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunSuiteLevelBeforeAllAfterAllHooksFireOnceScenario() {
    StepRegistry registry;

    // Live outside TestContext (a fresh TestContext per Scenario would hide
    // a cross-Scenario total), captured by reference - same rationale as
    // Scenario 4's "trail" and Scenario 30's "smokeNotSlowHookFireCount".
    int beforeAllFireCount = 0;
    int afterAllFireCount = 0;

    registry.AddBeforeAllHook([&beforeAllFireCount]() { ++beforeAllFireCount; });
    registry.AddAfterAllHook([&afterAllFireCount]() { ++afterAllFireCount; });

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
Feature: Suite-level Before-ALL/After-ALL hooks fire exactly once
  Scenario: First scenario
    Given a counter at 1
    When I add 1
    Then the counter should be 2

  Scenario: Second scenario
    Given a counter at 5
    When I add 5
    Then the counter should be 10

  Scenario: Third scenario
    Given a counter at 10
    When I add 10
    Then the counter should be 20
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/SuiteHooksFireOnce");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 3 && beforeAllFireCount == 1 &&
                             afterAllFireCount == 1;
    if (!asExpected) {
        std::cerr << "  SuiteLevelBeforeAllAfterAllHooksFireOnce: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " beforeAllFireCount=" << beforeAllFireCount << " afterAllFireCount=" << afterAllFireCount
                   << '\n';
    }
    return asExpected;
}

// -----_----_----_----_----_----_----_----_----_----_----_----_----_--
// Scenario 34: an After-ALL hook (StepRegistry::AddAfterAllHook) still
// fires even when one of the Feature's Scenarios FAILS - proving the
// documented run-even-on-failure guarantee (see RunFeature's "SAFETY
// CAVEAT" comment on the After-ALL loop in bdd.hpp), which only holds
// when onFailure does not exit/throw. Uses the same collecting
// FailureCollector pattern as RunMultiScenarioCollectFailuresScenario
// (Scenario 7) instead of the default exiting GherkinFailureCallback, so
// RunFeature actually reaches and runs the After-ALL loop instead of
// std::exit()ing on the first failing Scenario.
// -----_----_----_----_----_----_----_----_----_----_----_----_----_--

bool RunSuiteLevelAfterAllHookFiresOnScenarioFailureScenario() {
    StepRegistry registry;

    int afterAllFireCount = 0;
    registry.AddAfterAllHook([&afterAllFireCount]() { ++afterAllFireCount; });

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
Feature: After-ALL hook still fires when a Scenario fails
  Scenario: This one passes
    Given a counter at 1
    When I add 1
    Then the counter should be 2

  Scenario: This one fails
    Given a counter at 1
    When I add 1
    Then the counter should be 100
)feature";

    FailureCollector collector;
    const auto result =
        RunFeature(feature, registry, "SelfTestGherkin/SuiteAfterAllOnFailure", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 2 &&
                             result.scenarioResults[0].allPassed && !result.scenarioResults[1].allPassed &&
                             afterAllFireCount == 1 && collector.messages.size() == 1;
    if (!asExpected) {
        std::cerr << "  SuiteLevelAfterAllHookFiresOnScenarioFailure: allPassed=" << result.allPassed
                   << " scenarioResults.size()=" << result.scenarioResults.size()
                   << " afterAllFireCount=" << afterAllFireCount << " messages.size()=" << collector.messages.size()
                   << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// v0.9.0 Phase 1 coverage-gap closure (BBH gate): the scenarios below close
// bdd.hpp Gherkin coverage gaps found ONLY through the self-hosted BBH
// suite (the UT/gtest suite in test_Gherkin_Parser.cpp/test_Gherkin_
// Integration.cpp already independently covers the analogous cases -
// bdd.hpp being header-only means each TU compiles its own private copy,
// so BBH needed its OWN exercise of every one of these branches too). Every
// "malformed X" case below mirrors an already-existing UT-level test one to
// one; see that test for the exact bdd.hpp source location/rationale.
// ---------------------------------------------------------------------

// A dozen independent, top-level PARSE errors (impl::ParseFeatureText and
// its helpers) - every one of these fails before any Scenario ever runs, so
// result.featureName is empty and result.scenarioResults is empty, exactly
// like RunScenarioOutlineMalformedScenario's cases above.
bool RunStructuralParseErrorsScenario() {
    struct Case {
        std::string_view description;
        std::string_view feature;
        std::string_view expectedSubstring;
    };
    const std::array<Case, 14> cases{ {
        { "Multiple 'Feature:' sections", R"feature(
Feature: First
Feature: Second
)feature",
          "multiple 'Feature:' sections" },
        { "Step keyword before any Background:/Scenario:", R"feature(
Feature: Step too early
  Given this has nowhere to attach
)feature",
          "step found outside of a Background:/Scenario:" },
        { "Data table with no preceding step", R"feature(
Feature: Table too early
  Scenario: No step yet
    | a | b |
)feature",
          "data table with no preceding step" },
        { "Data table row cell count disagrees with its own header", R"feature(
Feature: Table row mismatch
  Scenario: Table grows a cell partway through
    Given a step
      | a | b |
      | 1 |
)feature",
          "cell(s), expected" },
        { "A second data table on the same step ('step already has an argument')", R"feature(
Feature: Two tables one step
  Scenario: Blank line between two table blocks
    Given a step
      | a |
      | 1 |

      | a |
      | 2 |
)feature",
          "step already has an argument" },
        { "Doc string with no preceding step", R"feature(
Feature: Doc string too early
  Scenario: No step yet
    """
    body
    """
)feature",
          "doc string with no preceding step" },
        { "A doc string on a step that already has a data table", R"feature(
Feature: Table then doc string
  Scenario: Data table wins the step's single argument slot
    Given a step
      | a |
      | 1 |
    """
    body
    """
)feature",
          "step already has an argument" },
        { "A second Examples: block's header disagrees with the first block's column count", R"feature(
Feature: Mismatched second Examples block
  Scenario Outline: Adding <n>
    Given step <n>

  Examples:
    | n |
    | 1 |

  Examples:
    | n | extra |
    | 2 | 3     |
)feature",
          "cell(s)" },
        { "Scenario Outline with no Examples, followed by another Scenario:", R"feature(
Feature: Missing examples before another Scenario
  Scenario Outline: No examples here
    Given a <thing>

  Scenario: The next one
    Given a step
)feature",
          "has no 'Examples:'/'Scenarios:' table" },
        { "Scenario Outline with no Examples, followed by a Background:", R"feature(
Feature: Missing examples before a Background
  Scenario Outline: No examples here
    Given a <thing>

  Background:
    Given a setup step
)feature",
          "has no 'Examples:'/'Scenarios:' table" },
        { "Doc string never closed before end of file", R"feature(
Feature: Unclosed doc string
  Scenario: Opens but never closes
    Given a step
    """
    body never closes
)feature",
          "doc string is not closed" },
        // Unlike the case right above (only discovered at EOF), this one
        // hits impl::ConsumeDocStringLine's own early corruptive detection:
        // a NEW 'Scenario:' line arrives before the closing '"""' - the
        // unclosed doc string is reported right there (against its OPENING
        // line) and parsing resumes normally with the new Scenario, instead
        // of silently swallowing it as more doc string content.
        { "Doc string unclosed, detected early at the next Scenario: boundary", R"feature(
Feature: Unclosed doc string hits a new Scenario before EOF
  Scenario: Opens but a new Scenario begins first
    Given a step
    """
    body never closes
  Scenario: A second scenario that parses normally
    Given a step
)feature",
          "doc string is not closed" },
        // Mirrors the case above but for a REJECTED doc string open attempt
        // (no preceding step) instead of a genuinely-open one - see
        // impl::ConsumeSkippedRejectedDocStringLine's own corruptive-resync
        // handling: a NEW 'Scenario:' line reached before the matching
        // closing '"""' stops the swallow and re-dispatches that boundary
        // line normally, rather than letting the rejected attempt eat the
        // whole next Scenario.
        { "Rejected doc string open swallow stops at a Scenario: boundary", R"feature(
Feature: Rejected doc string swallow stops at a block boundary
  Scenario: Doc string with no preceding step
    """
    stray content, never attached
  Scenario: A second scenario that parses normally
    Given a step
)feature",
          "doc string with no preceding step" },
        { "No 'Feature:' section anywhere in the text", "Just some free text, no Feature: line at all.\n",
          "no 'Feature:' found" },
    } };

    bool asExpected = true;
    for (const auto& testCase : cases) {
        StepRegistry registry;
        registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });
        registry.RegisterGiven("a setup step", [](TestContext&) -> bool { return true; });
        FailureCollector collector;
        const auto result = RunFeature(testCase.feature, registry, "SelfTestGherkin/StructuralParseErrors", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && result.featureName.empty() && result.scenarioResults.empty() &&
                                     collector.messages.size() == 1 &&
                                     collector.messages[0].find(testCase.expectedSubstring) != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  StructuralParseErrors[" << testCase.description << "]: allPassed=" << result.allPassed
                       << " featureName=\"" << result.featureName << "\" scenarioResults.size()=" << result.scenarioResults.size()
                       << " messages.size()=" << collector.messages.size() << '\n';
            for (const auto& message : collector.messages) {
                std::cerr << "    " << message << '\n';
            }
        }
    }
    return asExpected;
}

// impl::TokenizeTagExpression/impl::TagExpressionParser malformed-expression
// variants beyond the unbalanced-parens case
// RunTagExpressionParensAndMalformedThrowScenario already covers: a bare
// '@' with no tag name, an unrecognized keyword, a character the tokenizer
// doesn't understand at all, an empty/whitespace-only expression, and a
// missing operand (trailing "and"/leading "or"/lone "not"). Every one of
// these is a registration-time throw from AddBeforeHookExpr, never
// deferred to RunFeature().
bool RunTagExpressionMalformedVariantsScenario() {
    struct Case {
        std::string_view description;
        std::string_view expression;
    };
    const std::array<Case, 8> cases{ {
        { "Bare '@' with no tag name", "@" },
        { "'@' followed immediately by 'and'", "@ and @b" },
        { "Unrecognized keyword", "@a xor @b" },
        { "Tag written without its leading '@'", "a and b" },
        { "Character the tokenizer does not understand", "@a & @b" },
        { "Empty expression", "" },
        { "Whitespace-only expression", "   " },
        { "Trailing token after an otherwise-complete expression", "@a @b" },
    } };

    bool asExpected = true;
    for (const auto& testCase : cases) {
        StepRegistry registry;
        bool threw = false;
        try {
            registry.AddBeforeHookExpr(std::string(testCase.expression), [](TestContext&) {});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        const bool caseAsExpected = threw && registry.BeforeHooks().empty();
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  TagExpressionMalformedVariants[" << testCase.description << "]: threw=" << threw << '\n';
        }
    }

    // Also cover a lone "not" and a leading "or" with no left operand -
    // both are "missing operand" cases distinct from the trailing-"and"
    // case implicitly covered above via the leftover-token scan.
    for (const std::string_view expr : { "not", "or @a", "@a and" }) {
        StepRegistry registry;
        bool threw = false;
        try {
            registry.AddBeforeHookExpr(std::string(expr), [](TestContext&) {});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        asExpected = asExpected && threw;
        if (!threw) {
            std::cerr << "  TagExpressionMalformedVariants[missing operand \"" << expr << "\"]: did not throw\n";
        }
    }

    return asExpected;
}

// impl::UnescapePipeCell/impl::ParsePipeRow: a Data Table cell containing an
// escaped pipe ('\|', a literal pipe, not a column delimiter) and an
// escaped backslash ('\\', a literal backslash) - the UT-level analogue is
// test_Gherkin_Parser.cpp's DataTableCellsSupportBackslashEscapedPipeAndBackslash.
bool RunEscapedPipeAndBackslashInDataTableScenario() {
    StepRegistry registry;
    registry.RegisterGiven("the following notes:", [](TestContext& ctx, const DataTable& table) -> bool {
        ctx.Set("note0", table.Get(0, "text"));
        ctx.Set("note1", table.Get(1, "text"));
        return true;
    });
    registry.RegisterThen("note {int} should read {string}", [](TestContext& ctx, int index, std::string expected) -> bool {
        return ctx.Get<std::string>(index == 0 ? "note0" : "note1") == expected;
    });

    // Row 0's cell is the literal text `a|b` (escaped pipe, not a delimiter).
    // Row 1's cell is the literal text `a\b` (escaped backslash).
    constexpr std::string_view feature = R"feature(
Feature: Escaped pipe and backslash in data table cells
  Scenario: Two cells exercising both escape sequences
    Given the following notes:
      | text   |
      | a\|b   |
      | a\\b   |
    Then note 0 should read "a|b"
    Then note 1 should read "a\b"
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/EscapedPipeAndBackslash");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  EscapedPipeAndBackslashInDataTable: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// impl::ResolveStepTarget's `target.inBackground` branch - a Data Table
// attached to a BACKGROUND step (not a Scenario step). The UT-level
// analogue is test_Gherkin_Parser.cpp's DataTableAttachedToABackgroundStep.
bool RunBackgroundStepWithDataTableScenario() {
    StepRegistry registry;
    registry.RegisterGiven("the following accounts:", [](TestContext& ctx, const DataTable& table) -> bool {
        ctx.Set("accountCount", static_cast<int>(table.RowCount()));
        return true;
    });
    registry.RegisterThen("there should be {int} accounts", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("accountCount") == expected;
    });

    constexpr std::string_view feature = R"feature(
Feature: Data table on a Background step
  Background:
    Given the following accounts:
      | name  |
      | alice |
      | bob   |
  Scenario: The Background's own data table is resolved correctly
    Then there should be 2 accounts
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/BackgroundStepWithDataTable");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  BackgroundStepWithDataTable: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// impl::ProcessFeatureLine's final "ignorable free-text description line"
// fallthrough (also exercises impl::MatchStepKeyword's std::nullopt
// return): a plain prose line under a Scenario that isn't a tag, comment,
// table row, doc string delimiter, or step keyword.
bool RunFreeTextDescriptionLineScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Free text description lines
  This whole Feature does something useful, described here in prose.

  Scenario: Has its own free-text description too
    As a user, I want this scenario documented in plain English.
    Given a step
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/FreeTextDescription");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  FreeTextDescriptionLine: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// impl::ParseTimeoutValue's per-candidate-suffix skip branches: "ms" alone
// disqualifies BOTH the "ms" candidate (empty numberPart) and the "s"
// candidate (non-numeric numberPart "m") before falling through to the
// generic malformed-value error; a huge-but-int64-representable value with
// the "m" (minutes) suffix overflows the milliseconds conversion and is
// rejected by the dedicated overflow guard instead of wrapping via UB.
bool RunTimeoutUnitOnlyAndOverflowMalformedScenario() {
    struct Case {
        std::string_view description;
        std::string_view feature;
        std::string_view expectedSubstring;
    };
    const std::array<Case, 3> cases{ {
        { "Unit suffix with no digits before it ('ms' alone)", R"feature(
@timeout:ms
Feature: Timeout unit with no digits
  Scenario: A step that should never run
    Given a step that should never run
)feature",
          "malformed @timeout value" },
        { "A value that overflows the milliseconds conversion", R"feature(
@timeout:99999999999999999m
Feature: Timeout overflow
  Scenario: A step that should never run
    Given a step that should never run
)feature",
          "too large" },
        { "A zero-duration timeout value", R"feature(
@timeout:0s
Feature: Zero timeout
  Scenario: A step that should never run
    Given a step that should never run
)feature",
          "positive, non-zero duration" },
    } };

    bool asExpected = true;
    for (const auto& testCase : cases) {
        StepRegistry registry;
        int invocationCount = 0;
        registry.RegisterGiven("a step that should never run", [&invocationCount](TestContext&) -> bool {
            ++invocationCount;
            return true;
        });
        FailureCollector collector;
        const auto result = RunFeature(testCase.feature, registry, "SelfTestGherkin/TimeoutUnitOnlyAndOverflow", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && invocationCount == 0 && collector.messages.size() == 1 &&
                                     collector.messages[0].find(testCase.expectedSubstring) != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  TimeoutUnitOnlyAndOverflowMalformed[" << testCase.description << "]: allPassed=" << result.allPassed
                       << " invocationCount=" << invocationCount << " messages.size()=" << collector.messages.size() << '\n';
            for (const auto& message : collector.messages) {
                std::cerr << "    " << message << '\n';
            }
        }
    }
    return asExpected;
}

// impl::ParseRetryValue's two rejection branches: a non-numeric value, and
// an explicit zero (@retry:N is a TOTAL attempt count, so it cannot be
// zero) - both fail immediately, before any Before hook/Background/step
// ever runs, exactly like RunTimeoutAnnotationMalformedScenario above.
bool RunRetryMalformedAndZeroScenario() {
    struct Case {
        std::string_view description;
        std::string_view feature;
    };
    const std::array<Case, 2> cases{ {
        { "Non-numeric retry value", R"feature(
@retry:abc
Feature: Malformed retry value
  Scenario: A step that should never run
    Given a step that should never run
)feature" },
        { "Zero retry value", R"feature(
@retry:0
Feature: Zero retry value
  Scenario: A step that should never run
    Given a step that should never run
)feature" },
    } };

    bool asExpected = true;
    for (const auto& testCase : cases) {
        StepRegistry registry;
        int invocationCount = 0;
        registry.RegisterGiven("a step that should never run", [&invocationCount](TestContext&) -> bool {
            ++invocationCount;
            return true;
        });
        FailureCollector collector;
        const auto result = RunFeature(testCase.feature, registry, "SelfTestGherkin/RetryMalformedAndZero", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && invocationCount == 0 && collector.messages.size() == 1 &&
                                     collector.messages[0].find("@retry") != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  RetryMalformedAndZero[" << testCase.description << "]: allPassed=" << result.allPassed
                       << " invocationCount=" << invocationCount << " messages.size()=" << collector.messages.size() << '\n';
            for (const auto& message : collector.messages) {
                std::cerr << "    " << message << '\n';
            }
        }
    }
    return asExpected;
}

// impl::ConvertCapture<T>'s reference-return branch is reached by ANY
// ordinary {string}/{word} placeholder capture parameter declared `const
// std::string&` instead of `std::string` by value - not just a trailing
// Doc String raw argument (which never goes through ConvertCapture at all -
// see InvokeWithCapturesAndRaw). No trailing raw argument here at all
// (rawArgKind == None).
bool RunPlaceholderCaptureAsConstStringReferenceScenario() {
    StepRegistry registry;
    registry.RegisterGiven("the item is {string}", [](TestContext& ctx, const std::string& item) -> bool {
        ctx.Set("item", item);
        return true;
    });
    registry.RegisterThen("it should be recorded", [](TestContext& ctx) -> bool {
        return ctx.Get<std::string>("item") == "a widget";
    });

    constexpr std::string_view feature = R"feature(
Feature: Reference-typed placeholder capture
  Scenario: A {string} placeholder bound to const std::string&
    Given the item is "a widget"
    Then it should be recorded
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/ConstStringReferencePlaceholder");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  PlaceholderCaptureAsConstStringReference: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// impl::CompileStepPattern's unclosed-'{' fallback: NOT a registration-time
// error - everything from the unclosed '{' onward is escaped literal text
// instead, so the pattern only ever matches that exact literal.
bool RunUnclosedPlaceholderBraceFallsBackToLiteralScenario() {
    StepRegistry registry;
    registry.RegisterGiven("I have {int apples", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Unclosed placeholder brace
  Scenario: The literal text (including the stray '{') is matched verbatim
    Given I have {int apples
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/UnclosedPlaceholderBrace");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  UnclosedPlaceholderBraceFallsBackToLiteral: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// "Narrow misuse" regression (see impl::InvokeStepNoRawArg's own comment): a
// step definition whose ONLY parameter (besides TestContext&) is typed
// exactly `DataTable`, registered against a pattern whose placeholder count
// equals that parameter count EXACTLY (not +1) - registration succeeds
// (only COUNTS are compared), but invocation falls through to a defensive
// internal-error step failure instead of ever calling
// ConvertCapture<DataTable> (which would not even compile).
bool RunDataTableAsPlainPlaceholderNarrowMisuseScenario() {
    StepRegistry registry;
    registry.RegisterGiven("{int} things happen", [](TestContext&, DataTable table) -> bool {
        return table.RowCount() == 0;
    });

    constexpr std::string_view feature = R"feature(
Feature: DataTable parameter miscounted as a placeholder
  Scenario: Placeholder count matches but the type cannot really convert
    Given 5 things happen
)feature";

    FailureCollector collector;
    const auto result = RunFeature(feature, registry, "SelfTestGherkin/DataTableAsPlainPlaceholderNarrowMisuse", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 && collector.messages.size() == 1 &&
                             collector.messages[0].find("Precondition failed") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  DataTableAsPlainPlaceholderNarrowMisuse: allPassed=" << result.allPassed
                   << " messages.size()=" << collector.messages.size() << '\n';
        for (const auto& message : collector.messages) {
            std::cerr << "    " << message << '\n';
        }
    }
    return asExpected;
}

// impl::InvokeStepWithRawArg's "expects a data table/doc string but none is
// attached" branches: a step definition declares a trailing raw-argument
// parameter (const DataTable&/const std::string&), but the matched
// .feature step doesn't actually attach the corresponding block at all.
bool RunRawArgumentDeclaredButNotAttachedScenario() {
    bool asExpected = true;

    {
        StepRegistry registry;
        registry.RegisterGiven("the following items", [](TestContext&, const DataTable& table) -> bool {
            return table.RowCount() == 0;
        });
        constexpr std::string_view feature = R"feature(
Feature: Data table parameter but no table attached
  Scenario: No '|' rows follow this step
    Given the following items
)feature";
        FailureCollector collector;
        const auto result = RunFeature(feature, registry, "SelfTestGherkin/DataTableNotAttached", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && collector.messages.size() == 1 &&
                                     collector.messages[0].find("Precondition failed") != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  RawArgumentDeclaredButNotAttached[DataTable]: allPassed=" << result.allPassed << '\n';
        }
    }
    {
        StepRegistry registry;
        registry.RegisterGiven("the article body is:", [](TestContext&, const std::string& body) -> bool {
            return body.empty();
        });
        constexpr std::string_view feature = R"feature(
Feature: Doc string parameter but no doc string attached
  Scenario: No '"""' block follows this step
    Given the article body is:
)feature";
        FailureCollector collector;
        const auto result = RunFeature(feature, registry, "SelfTestGherkin/DocStringNotAttached", collector.AsCallback());
        const bool caseAsExpected = !result.allPassed && collector.messages.size() == 1 &&
                                     collector.messages[0].find("Precondition failed") != std::string::npos;
        asExpected = asExpected && caseAsExpected;
        if (!caseAsExpected) {
            std::cerr << "  RawArgumentDeclaredButNotAttached[DocString]: allPassed=" << result.allPassed << '\n';
        }
    }

    return asExpected;
}

// impl::StepDefinitionRawArgKind's registration-time throw: a trailing
// parameter beyond the placeholder captures that is neither exactly `const
// DataTable&` nor exactly `const std::string&`. The UT-level analogue is
// test_Gherkin_Integration.cpp's RegisteringATrailingParameterThatIsNeitherDataTableNorStringThrows.
bool RunInvalidTrailingParameterTypeThrowsScenario() {
    StepRegistry registry;
    bool threw = false;
    std::string message;
    try {
        registry.RegisterGiven("a thing happens", [](TestContext&, int extra) -> bool { return extra == 0; });
    } catch (const std::invalid_argument& e) {
        threw = true;
        message = e.what();
    }
    const bool asExpected = threw && message.find("must be exactly 'const DataTable&' or 'const std::string&'") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  InvalidTrailingParameterTypeThrows: threw=" << threw << " message=\"" << message << "\"\n";
    }
    return asExpected;
}

// impl::SubstitutePlaceholders's "no closing '>'" branch: a '<' with no
// matching '>' anywhere after it in an Outline step's text is left as
// literal text (the whole remainder, verbatim). The UT-level analogue is
// test_Gherkin_Parser.cpp's UnterminatedAngleBracketInOutlineStepIsLeftAsLiteralText.
bool RunUnterminatedAngleBracketPlaceholderScenario() {
    StepRegistry registry;
    registry.RegisterGiven("a value of <x and more", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Unterminated placeholder
  Scenario Outline: Has a stray '<' with no closing '>'
    Given a value of <x and more

  Examples:
    | x |
    | 1 |
)feature";

    const auto result = RunFeature(feature, registry, "SelfTestGherkin/UnterminatedAngleBracket");
    const bool asExpected = result.allPassed && result.scenarioResults.size() == 1;
    if (!asExpected) {
        std::cerr << "  UnterminatedAngleBracketPlaceholder: allPassed=" << result.allPassed << '\n';
    }
    return asExpected;
}

// A well-formed '<name>' placeholder (has a closing '>') whose name simply
// isn't one of the Examples header columns - distinct from
// RunUnterminatedAngleBracketPlaceholderScenario's "no closing '>' at all"
// case. impl::SubstitutePlaceholders leaves such a placeholder as literal
// text ("no matching column: left literal"), so the expanded step text
// still contains the literal "<undefined>" token, which then fails to match
// any registered step definition - a reported failure, not a crash.
bool RunOutlinePlaceholderWithNoMatchingColumnScenario() {
    // Note: {word} expands to (\S+), which would happily match a literal
    // "<undefined>" token, masking the "left literal" behavior as a pass
    // rather than a mismatch. Use a fixed, placeholder-free step pattern
    // instead, so the substituted literal "<undefined>" text fails to match
    // the registered step text exactly - proving the placeholder really was
    // left untouched rather than silently swallowed.
    StepRegistry registry;
    registry.RegisterGiven("a value of exactly known", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"feature(
Feature: Outline placeholder with no matching column
  Scenario Outline: References a column that Examples never defines
    Given a value of <undefined>

  Examples:
    | x |
    | 1 |
)feature";

    FailureCollector collector;
    const auto result =
        RunFeature(feature, registry, "SelfTestGherkin/OutlinePlaceholderNoMatchingColumn", collector.AsCallback());

    const bool asExpected = !result.allPassed && result.scenarioResults.size() == 1 && collector.messages.size() == 1 &&
                             collector.messages[0].find("<undefined>") != std::string::npos;
    if (!asExpected) {
        std::cerr << "  OutlinePlaceholderWithNoMatchingColumn: allPassed=" << result.allPassed
                   << " messages.size()=" << collector.messages.size() << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Internal defensive guards (v0.9.1 100%-coverage closure): a handful of
// impl::-level branches this codebase's own comments already document as
// "unreachable in practice" - defensive invariant-guards kept only so the
// surrounding code compiles/never dereferences something empty, never
// actually reachable through StepRegistry::AddStepDefinition's registration-
// time validation or ParseFeatureText's own state machine. Called here
// directly with deliberately invariant-violating state - the UT-level
// analogue is test_Gherkin_Parser.cpp's own GherkinInternalDefensiveGuards
// test suite, which this mirrors one-for-one so BOTH independent coverage
// measurements (UT and BBH - see this file's own header comment) reach
// these lines, not just one of them.
// ---------------------------------------------------------------------

bool RunInternalDefensiveGuardsScenario() {
    bool asExpected = true;

    // impl::InvokeStepNoRawArg's "captured argument count does not match
    // step definition" branch: unreachable via AddStepDefinition/TryMatch,
    // which always keep captures.size() and paramCount in lockstep whenever
    // rawArgKind == RawArgumentKind::None (AddStepDefinition throws at
    // registration time otherwise).
    {
        auto stepFn = [](TestContext&, int) -> bool { return true; };
        using F = decltype(stepFn);
        using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
        TestContext ctx;
        const std::vector<std::string> captures; // size 0, but paramCount is 1.
        const bool caseOk = !GherkinImpl::InvokeStepNoRawArg<ArgsTuple, 1, false>(stepFn, ctx, captures);
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[InvokeStepNoRawArg mismatch]: expected false\n";
        }
    }

    // impl::InvokeStepWithRawArg's two "captured argument count does not
    // match step definition" branches (DataTable-sink and std::string-sink).
    {
        auto stepFn = [](TestContext&, int, const DataTable&) -> bool { return true; };
        using F = decltype(stepFn);
        using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
        TestContext ctx;
        const std::vector<std::string> captures; // size 0, but capturesN (paramCount - 1) is 1.
        const GherkinImpl::RawArgument rawArgument{};
        const bool caseOk = !GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 2>(stepFn, ctx, captures, rawArgument);
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[InvokeStepWithRawArg DataTable mismatch]: expected false\n";
        }
    }
    {
        auto stepFn = [](TestContext&, int, const std::string&) -> bool { return true; };
        using F = decltype(stepFn);
        using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
        TestContext ctx;
        const std::vector<std::string> captures; // size 0, but capturesN (paramCount - 1) is 1.
        const GherkinImpl::RawArgument rawArgument{};
        const bool caseOk = !GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 2>(stepFn, ctx, captures, rawArgument);
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[InvokeStepWithRawArg std::string mismatch]: expected false\n";
        }
    }

    // impl::InvokeStepWithRawArg's final "not a recognized raw-argument
    // type" arm: StepDefinitionRawArgKind only ever returns a non-None kind
    // when F's real trailing parameter is DataTable or std::string (else it
    // throws at registration time), so InvokeStepWithRawArg is never
    // actually CALLED for an F like this one through AddStepDefinition/
    // MakeStepThunk - even though it IS instantiated (MakeStepThunk's lambda
    // body references both branches unconditionally for every F). This is
    // an ordinary placeholder-only step definition (last parameter is plain
    // int), used all over this codebase; this calls its already-compiled
    // InvokeStepWithRawArg instantiation directly instead of through that
    // lambda.
    {
        auto stepFn = [](TestContext&, int) -> bool { return true; };
        using F = decltype(stepFn);
        using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
        TestContext ctx;
        const std::vector<std::string> captures{ "5" };
        const GherkinImpl::RawArgument rawArgument{};
        const bool caseOk = !GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 1>(stepFn, ctx, captures, rawArgument);
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[InvokeStepWithRawArg unrecognized type]: expected false\n";
        }
    }

    // impl::InvokeStepWithRawArg's "paramCount == 0" else-branch:
    // AddStepDefinition only ever computes a non-None rawArgKind when
    // paramCount >= 1, so a zero-parameter step definition (very common - a
    // Given/When/Then with no placeholder captures at all) always keeps
    // rawArgKind == None, and this runtime `if (rawArgKind == None) return
    // InvokeStepNoRawArg...` branch in MakeStepThunk's lambda never even
    // calls InvokeStepWithRawArg for it - but the function IS still
    // instantiated (and compiled) for such an F, same reasoning as above.
    {
        auto stepFn = [](TestContext&) -> bool { return true; };
        using F = decltype(stepFn);
        using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
        TestContext ctx;
        const std::vector<std::string> captures;
        const GherkinImpl::RawArgument rawArgument{};
        const bool caseOk = !GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 0>(stepFn, ctx, captures, rawArgument);
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[InvokeStepWithRawArg zero param count]: expected false\n";
        }
    }

    // impl::ResolveStepTarget's "no Scenario in progress" throw: unreachable
    // via ProcessFeatureLine, which always resets lastStepTarget before
    // currentScenario is ever swapped/flushed - so a StepTarget with
    // inBackground == false is only ever produced while currentScenario is
    // engaged.
    {
        GherkinImpl::FeatureParseState state;
        const GherkinImpl::StepTarget target{ .inBackground = false, .index = 0 };
        bool threw = false;
        try {
            std::ignore = GherkinImpl::ResolveStepTarget(state, target);
        } catch (const std::logic_error&) {
            threw = true;
        }
        asExpected = asExpected && threw;
        if (!threw) {
            std::cerr << "  InternalDefensiveGuards[ResolveStepTarget]: expected std::logic_error\n";
        }
    }

    // impl::HandleDataTableLine's "data table with no preceding step" branch
    // reached while state.inDataTable is already true: unreachable via
    // ProcessFeatureLine, which only ever sets inDataTable true right after
    // lastStepTarget is set (both cleared together on every context change).
    {
        GherkinImpl::FeatureParseState state;
        state.inDataTable = true;
        state.lastStepTarget.reset();
        GherkinImpl::HandleDataTableLine(state, "| a | b |", 7);
        const bool caseOk = state.errors.size() == 1 &&
                             state.errors.front().find("data table with no preceding step") != std::string::npos &&
                             !state.inDataTable && state.skipMalformedTableLines;
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[HandleDataTableLine]: errors.size()=" << state.errors.size() << '\n';
        }
    }

    // impl::HandleExamplesTableRow's "!pendingExamples" auto-init guard:
    // unreachable via ProcessFeatureLine, which only ever routes here while
    // state.inExamplesTable is true, which is only ever set true together
    // with state.pendingExamples being engaged.
    {
        GherkinImpl::FeatureParseState state;
        state.pendingExamples.reset();
        GherkinImpl::HandleExamplesTableRow(state, "| a | b |", 3);
        const bool caseOk = state.pendingExamples.has_value() &&
                            state.pendingExamples->header == std::vector<std::string>{ "a", "b" } &&
                            state.haveExamplesHeader;
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[HandleExamplesTableRow]: pendingExamples.has_value()="
                       << state.pendingExamples.has_value() << '\n';
        }
    }

    // impl::HandleDocStringLine's closing-delimiter "no preceding step"
    // guard: unreachable via ProcessFeatureLine, since reaching a CLOSING
    // '"""' with state.inDocString true implies the OPENING '"""' already
    // validated state.lastStepTarget (the only place state.inDocString is
    // ever set true).
    {
        GherkinImpl::FeatureParseState state;
        state.inDocString = true;
        state.docStringLines = { "orphaned content" };
        state.lastStepTarget.reset();
        GherkinImpl::HandleDocStringLine(state, R"(""")", R"(""")", 9);
        const bool caseOk = state.errors.size() == 1 &&
                             state.errors.front().find("doc string with no preceding step") != std::string::npos &&
                             !state.inDocString && state.docStringLines.empty();
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[HandleDocStringLine]: errors.size()=" << state.errors.size() << '\n';
        }
    }

    // impl::EvaluateTagExpression's exhaustive-switch trailing "return
    // false": every TagExpressionNode ParseTagExpression ever produces has
    // op set to one of the 4 real enumerators, so this is genuinely
    // unreachable via any parser-built node - only a corrupted op value
    // (impossible via ParseTagExpression itself) falls through to it.
    {
        const GherkinImpl::TagExpressionNode node{
            .op = static_cast<GherkinImpl::TagExprOp>(99), .tagName = "", .left = nullptr, .right = nullptr };
        const bool caseOk = !GherkinImpl::EvaluateTagExpression(node, {});
        asExpected = asExpected && caseOk;
        if (!caseOk) {
            std::cerr << "  InternalDefensiveGuards[EvaluateTagExpression]: expected false\n";
        }
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
    ReportScenario("ScenarioOutlineExpansion: Outline + Examples expands into one Scenario per row",
                    RunScenarioOutlineExpansionScenario(), passCount, totalCount);
    ReportScenario("ScenarioOutlineMalformed: the 5 real Outline/Examples parse-error conditions",
                    RunScenarioOutlineMalformedScenario(), passCount, totalCount);
    ReportScenario("ScenarioOutlineMultipleExamples: two Examples: blocks on one Outline merge all rows",
                    RunScenarioOutlineMultipleExamplesBlocksScenario(), passCount, totalCount);
    ReportScenario("TimeoutAnnotationNoTrip: a generous @timeout never trips",
                    RunTimeoutAnnotationNoTripScenario(), passCount, totalCount);
    ReportScenario("TimeoutAnnotationTrip: a tight @timeout trips on the next step's precheck",
                    RunTimeoutAnnotationTripScenario(), passCount, totalCount);
    ReportScenario("TimeoutAnnotationMalformed: a bare unitless @timeout value is a parse error",
                    RunTimeoutAnnotationMalformedScenario(), passCount, totalCount);
    ReportScenario("ParallelScenariosInOriginalOrder: enableParallelScenarios preserves declaration order",
                    RunParallelScenariosInOriginalOrderScenario(), passCount, totalCount);
    ReportScenario("ButStepKeyword: Given/When/And/But chain, But is a distinct registration method",
                    RunButStepKeywordScenario(), passCount, totalCount);
    ReportScenario("BeforeHookWithActiveTimeout: a Before hook under a generous, non-tripping @timeout",
                    RunBeforeHookWithActiveTimeoutScenario(), passCount, totalCount);
    ReportScenario("DataTableBasic: a table-only step definition (no placeholder captures)",
                    RunDataTableBasicScenario(), passCount, totalCount);
    ReportScenario("DataTableWithCapture: a {word} capture plus a trailing data table",
                    RunDataTableWithCaptureScenario(), passCount, totalCount);
    ReportScenario("DataTableGetUnknownColumnThrows: DataTable::Get on an unknown column",
                    RunDataTableGetUnknownColumnThrowsScenario(), passCount, totalCount);
    ReportScenario("DocStringBasic: a doc-string-only step definition, blank line + indentation preserved",
                    RunDocStringBasicScenario(), passCount, totalCount);
    ReportScenario("DocStringWithCapture: a {word} capture plus a trailing doc string",
                    RunDocStringWithCaptureScenario(), passCount, totalCount);
    ReportScenario("RetryScenarioSucceedsAfterInitialFailure: @retry recovers from transient failure",
                    RunRetryScenarioSucceedsAfterInitialFailureScenario(), passCount, totalCount);
    ReportScenario("RetryScenarioAllAttemptsFail: @retry with persistent failure reported exactly once",
                    RunRetryScenarioAllAttemptsFailScenario(), passCount, totalCount);
    ReportScenario("TagExpressionHookAndNot: AddBeforeHookExpr \"@smoke and not @slow\" fires only where satisfied",
                    RunTagExpressionHookAndNotScenario(), passCount, totalCount);
    ReportScenario("VectorAndExpressionHooksCoexist: AddBeforeHook + AddBeforeHookExpr on one registry, no cross-interference",
                    RunVectorAndExpressionHooksCoexistScenario(), passCount, totalCount);
    ReportScenario("TagExpressionParensAndMalformedThrow: parenthesized grouping fires end-to-end + malformed AddBeforeHookExpr throws",
                    RunTagExpressionParensAndMalformedThrowScenario(), passCount, totalCount);
    ReportScenario("SuiteLevelBeforeAllAfterAllHooksFireOnce: AddBeforeAllHook/AddAfterAllHook fire once per Feature, not per Scenario",
                    RunSuiteLevelBeforeAllAfterAllHooksFireOnceScenario(), passCount, totalCount);
    ReportScenario("SuiteLevelAfterAllHookFiresOnScenarioFailure: AddAfterAllHook still fires when a Scenario fails",
                    RunSuiteLevelAfterAllHookFiresOnScenarioFailureScenario(), passCount, totalCount);

    // --- v0.9.0 Phase 1 coverage-gap closure (BBH gate) ---
    ReportScenario("StructuralParseErrors: 12 independent top-level .feature parse errors",
                    RunStructuralParseErrorsScenario(), passCount, totalCount);
    ReportScenario("TagExpressionMalformedVariants: bare '@', unknown keyword, bad char, empty, missing operand",
                    RunTagExpressionMalformedVariantsScenario(), passCount, totalCount);
    ReportScenario("EscapedPipeAndBackslashInDataTable: '\\|' and '\\\\' inside a data table cell",
                    RunEscapedPipeAndBackslashInDataTableScenario(), passCount, totalCount);
    ReportScenario("BackgroundStepWithDataTable: a data table attached to a Background step",
                    RunBackgroundStepWithDataTableScenario(), passCount, totalCount);
    ReportScenario("FreeTextDescriptionLine: prose lines under Feature:/Scenario: are ignored",
                    RunFreeTextDescriptionLineScenario(), passCount, totalCount);
    ReportScenario("TimeoutUnitOnlyAndOverflowMalformed: 'ms' alone, an overflowing value, and a zero-duration value",
                    RunTimeoutUnitOnlyAndOverflowMalformedScenario(), passCount, totalCount);
    ReportScenario("RetryMalformedAndZero: a non-numeric and a zero @retry value",
                    RunRetryMalformedAndZeroScenario(), passCount, totalCount);
    ReportScenario("PlaceholderCaptureAsConstStringReference: a {string} capture bound to const std::string&",
                    RunPlaceholderCaptureAsConstStringReferenceScenario(), passCount, totalCount);
    ReportScenario("UnclosedPlaceholderBraceFallsBackToLiteral: a step pattern with an unclosed '{'",
                    RunUnclosedPlaceholderBraceFallsBackToLiteralScenario(), passCount, totalCount);
    ReportScenario("DataTableAsPlainPlaceholderNarrowMisuse: a DataTable-typed parameter miscounted as a placeholder",
                    RunDataTableAsPlainPlaceholderNarrowMisuseScenario(), passCount, totalCount);
    ReportScenario("RawArgumentDeclaredButNotAttached: trailing DataTable/DocString parameter with nothing attached",
                    RunRawArgumentDeclaredButNotAttachedScenario(), passCount, totalCount);
    ReportScenario("InvalidTrailingParameterTypeThrows: a trailing parameter that is neither DataTable nor string",
                    RunInvalidTrailingParameterTypeThrowsScenario(), passCount, totalCount);
    ReportScenario("UnterminatedAngleBracketPlaceholder: a '<' with no closing '>' in an Outline step",
                    RunUnterminatedAngleBracketPlaceholderScenario(), passCount, totalCount);
    ReportScenario("OutlinePlaceholderWithNoMatchingColumn: a well-formed '<name>' not in the Examples header",
                    RunOutlinePlaceholderWithNoMatchingColumnScenario(), passCount, totalCount);
    ReportScenario("InternalDefensiveGuards: impl:: branches unreachable in practice, called directly (white-box)",
                    RunInternalDefensiveGuardsScenario(), passCount, totalCount);

    std::cout << '\n' << passCount << "/" << totalCount << " Gherkin scenarios behaved as expected\n";

    return (passCount == totalCount) ? EXIT_SUCCESS : EXIT_FAILURE;
}
