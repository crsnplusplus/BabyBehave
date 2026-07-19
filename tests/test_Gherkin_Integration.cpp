// test_Gherkin_Integration.cpp
//
// End-to-end tests of BabyBehave::BDD::Gherkin: StepRegistry + RunFeature()
// driving real BabyBehaveTest/TestContext execution, covering every
// documented behavior (see docs/design/gherkin-support.md) - as opposed to
// test_Gherkin_Parser.cpp, which tests the ".feature" text parser and
// cucumber-expression-lite pattern compiler in isolation via impl::.
//
// RunFeature() is fail-hard: a malformed feature or any scenario that
// doesn't pass makes it print a diagnostic and call std::exit(EXIT_FAILURE)
// (see bdd.hpp's RunFeature()/impl::ReportScenarioFailureAndExit()) - it
// never returns in either case. Every test that exercises one of those
// paths therefore uses gtest's EXPECT_EXIT (see the GherkinIntegrationDeathTest
// suite below), following the same pattern already used in this repo's
// tests/test_BabyBehaveTest_CallbacksAndDeathTests.cpp for BabyBehaveTest's
// own default exit-on-failure callbacks. Every other test below is a
// genuinely PASSING scenario, so RunFeature() returns normally and its
// FeatureResult/TestResult can be inspected directly - which is also how
// StepResult::location is checked without needing a death test at all
// (bdd.hpp's forced collect-failures mode records a StepResult, including
// its location, for every step, pass or fail - see VerifyCondition()).
#include <BabyBehave/bdd.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;
namespace GherkinImpl = BabyBehave::BDD::Gherkin::impl;

// ---------------------------------------------------------------------
// Step-definition matching: exact text + {int}/{float}/{string}/{word}
// placeholders, passed through as typed arguments.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, BasicScenarioWithTypedPlaceholdersPasses) {
    StepRegistry registry;

    registry.RegisterGiven("an empty crate", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I add {int} apples, {float} kg of oranges labeled {string} tagged {word}",
                           [](TestContext& ctx, int apples, double kg, std::string label, std::string tag) -> bool {
                               ctx.Set("apples", apples);
                               ctx.Set("kg", kg);
                               ctx.Set("label", label);
                               ctx.Set("tag", tag);
                               return true;
                           });
    registry.RegisterThen(
        "the crate holds {int} apples, {float} kg, labeled {string}, tagged {word}",
        [](TestContext& ctx, int apples, double kg, std::string label, std::string tag) -> bool {
            return ctx.Get<int>("apples") == apples && std::abs(ctx.Get<double>("kg") - kg) < 1e-9 &&
                   ctx.Get<std::string>("label") == label && ctx.Get<std::string>("tag") == tag;
        });

    constexpr std::string_view feature = R"FEATURE(
Feature: Placeholder types
  Scenario: All four placeholder kinds in one step
    Given an empty crate
    When I add 3 apples, 2.5 kg of oranges labeled "fresh fruit" tagged batch42
    Then the crate holds 3 apples, 2.5 kg, labeled "fresh fruit", tagged batch42
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

TEST(GherkinIntegration, NegativeIntAndFloatPlaceholdersParseCorrectly) {
    StepRegistry registry;
    registry.RegisterGiven("the current temperature is {int} degrees", [](TestContext& ctx, int t) -> bool {
        ctx.Set("temp", t);
        return true;
    });
    registry.RegisterWhen("the temperature drops by {int} degrees", [](TestContext& ctx, int delta) -> bool {
        ctx.Set("temp", ctx.Get<int>("temp") + delta);
        return true;
    });
    registry.RegisterThen("the temperature is {int} degrees", [](TestContext& ctx, int expected) -> bool {
        return ctx.Get<int>("temp") == expected;
    });
    registry.RegisterAnd("the change ratio is {float}", [](TestContext&, double ratio) -> bool {
        return std::abs(ratio - (-2.5)) < 1e-9;
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Negative numbers
  Scenario: Temperature drop
    Given the current temperature is 5 degrees
    When the temperature drops by -10 degrees
    Then the temperature is -5 degrees
    And the change ratio is -2.5
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");
    EXPECT_TRUE(result.allPassed);
}

// ---------------------------------------------------------------------
// And/But steps are matched against their OWN registered category
// (RegisterAnd/RegisterBut), never against whichever category the
// preceding line happened to be - see the mapping table in
// docs/design/gherkin-support.md and impl::StepRegistry::m_definitions'
// comment. Each decoy registration below is deliberately WRONG (returns
// false) so that if the interpreter ever fell back to matching "And"/"But"
// text against Given's definitions, this test would actually fail instead
// of passing regardless.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, AndButKeywordsMatchTheirOwnCategoryNotThePrecedingContext) {
    StepRegistry registry;

    registry.RegisterGiven("the scenario starts", [](TestContext&) -> bool { return true; });

    // Decoys: same step text registered under Given, but wrong (failing).
    // Only reached if And/But routing incorrectly falls back to Given.
    registry.RegisterGiven("this and-step must use the And category", [](TestContext&) -> bool { return false; });
    registry.RegisterGiven("this but-step must use the But category", [](TestContext&) -> bool { return false; });

    // Correct registrations, under their own real category.
    registry.RegisterAnd("this and-step must use the And category", [](TestContext&) -> bool { return true; });
    registry.RegisterBut("this but-step must use the But category", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: And/But category routing
  Scenario: And and But steps use their own registry category
    Given the scenario starts
    And this and-step must use the And category
    But this but-step must use the But category
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");
    EXPECT_TRUE(result.allPassed);
}

// ---------------------------------------------------------------------
// Background: prepended to every Scenario in the same Feature, with
// per-step attribution kept granular (a Background step failing/passing
// shows up as its own StepResult, not collapsed into one aggregate
// "ContextSetup" outcome).
// ---------------------------------------------------------------------

TEST(GherkinIntegration, BackgroundStepsArePrependedToEveryScenarioWithGranularAttribution) {
    StepRegistry registry;
    registry.RegisterGiven("a freshly booted coffee machine", [](TestContext& ctx) -> bool {
        ctx.Set("booted", true);
        return true;
    });
    registry.RegisterAnd("a full water tank", [](TestContext& ctx) -> bool {
        ctx.Set("tankFull", true);
        return true;
    });
    registry.RegisterWhen("I brew an espresso", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("a cup is served", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("booted") && ctx.Get<bool>("tankFull");
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Coffee machine
  Background:
    Given a freshly booted coffee machine
    And a full water tank
  Scenario: Brewing an espresso
    When I brew an espresso
    Then a cup is served
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    ASSERT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    const TestResult& scenario = result.scenarioResults[0];

    // Background's 2 steps + the Scenario's own 2 steps = 4 distinct,
    // individually-attributed StepResults - not one aggregate entry.
    ASSERT_EQ(scenario.steps.size(), 4u);
    EXPECT_EQ(scenario.steps[0].stepName, "[Background] a freshly booted coffee machine");
    EXPECT_EQ(scenario.steps[0].stepLabel, "Precondition");
    EXPECT_TRUE(scenario.steps[0].passed);
    EXPECT_EQ(scenario.steps[1].stepName, "[Background] a full water tank");
    EXPECT_EQ(scenario.steps[1].stepLabel, "And");
    EXPECT_TRUE(scenario.steps[1].passed);
    EXPECT_EQ(scenario.steps[2].stepName, "I brew an espresso");
    EXPECT_EQ(scenario.steps[3].stepName, "a cup is served");
}

// ---------------------------------------------------------------------
// Tag inheritance: a Scenario's effective tags are the union of its own
// tags and its Feature's tags. Verified via a Before hook that only fires
// for the union (feature tag AND scenario tag together): if inheritance
// weren't applied, hookCount below would stay at 0 for BOTH scenarios,
// since neither scenario declares both tags on its own.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, TagUnionMakesFeatureLevelTagsInheritedByEveryScenario) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto featureTagOnlyCount = std::make_shared<int>(0);
    auto bothTagsCount = std::make_shared<int>(0);

    // Requires ONLY the Feature-level tag: must fire for every Scenario in
    // the Feature, proving the Feature tag reaches Scenario-level matching
    // at all.
    registry.AddBeforeHook({"featuretag"}, [featureTagOnlyCount](TestContext&) { ++(*featureTagOnlyCount); });

    // Requires BOTH the Feature-level tag and a tag only Scenario A
    // declares: must fire for A only, proving the union (not just the
    // Scenario's own tags, and not just the Feature's) is what gets
    // checked.
    registry.AddBeforeHook({"featuretag", "scenariotag"},
                            [bothTagsCount](TestContext&) { ++(*bothTagsCount); });

    constexpr std::string_view feature = R"FEATURE(
@featuretag
Feature: Tag inheritance
  @scenariotag
  Scenario: A - has its own scenario tag too
    Given a step

  Scenario: B - relies purely on inherited Feature tag
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_EQ(*featureTagOnlyCount, 2) << "Feature-level tag must be inherited by every Scenario";
    EXPECT_EQ(*bothTagsCount, 1) << "Union-tag hook must fire only for the Scenario with the extra tag";
}

// ---------------------------------------------------------------------
// Hook tag matching is AND/subset-only: every required tag must be
// present. A hook whose tags match no Scenario is silent, NOT an error.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, HookTagMatchIsAndSubsetNotJustAnyOverlap) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto count = std::make_shared<int>(0);
    // Requires BOTH tags; the single tagged scenario below only has one of
    // them, so this must never fire (an "any overlap"/OR match would fire
    // here incorrectly).
    registry.AddBeforeHook({"slow", "integration"}, [count](TestContext&) { ++(*count); });

    constexpr std::string_view feature = R"FEATURE(
Feature: AND-only tag matching
  @slow
  Scenario: Only one of the two required tags
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    EXPECT_EQ(*count, 0);
}

TEST(GherkinIntegration, HookMatchingNoScenarioIsNotAnError) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto count = std::make_shared<int>(0);
    registry.AddBeforeHook({"typo-tag-matching-nothing"}, [count](TestContext&) { ++(*count); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Unmatched hook
  Scenario: Untagged
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    EXPECT_EQ(*count, 0);
}

TEST(GherkinTagHelpers, UnionTagsDeduplicatesAndPreservesOrder) {
    const std::vector<std::string> featureTags{"a", "b"};
    const std::vector<std::string> scenarioTags{"b", "c"};
    const std::vector<std::string> unioned = GherkinImpl::UnionTags(featureTags, scenarioTags);
    ASSERT_EQ(unioned.size(), 3u);
    EXPECT_EQ(unioned[0], "a");
    EXPECT_EQ(unioned[1], "b");
    EXPECT_EQ(unioned[2], "c");
}

TEST(GherkinTagHelpers, TagsAreSubsetOfIsAndNotOr) {
    const std::vector<std::string> available{"slow"};
    EXPECT_TRUE(GherkinImpl::TagsAreSubsetOf({}, available)) << "empty requirement is vacuously true";
    EXPECT_TRUE(GherkinImpl::TagsAreSubsetOf({"slow"}, available));
    EXPECT_FALSE(GherkinImpl::TagsAreSubsetOf({"slow", "integration"}, available))
        << "AND semantics: ALL required tags must be present, not just one";
}

// ---------------------------------------------------------------------
// Execution order per scenario: Before hooks (registration order) ->
// Background -> Scenario's own steps -> After hooks (registration order).
// A shared log, appended to by every hook/step, pins this down precisely -
// any reordering (e.g. Background before Before hooks, or After hooks
// running in reverse/stack order) would change the final vector and fail
// the EXPECT_EQ below.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, ExecutionOrderIsBeforeThenBackgroundThenStepsThenAfter) {
    auto log = std::make_shared<std::vector<std::string>>();

    StepRegistry registry;
    registry.RegisterGiven("a background step", [log](TestContext&) -> bool {
        log->push_back("background");
        return true;
    });
    registry.RegisterWhen("scenario step one", [log](TestContext&) -> bool {
        log->push_back("step1");
        return true;
    });
    registry.RegisterThen("scenario step two", [log](TestContext&) -> bool {
        log->push_back("step2");
        return true;
    });
    registry.AddBeforeHook({}, [log](TestContext&) { log->push_back("before1"); });
    registry.AddBeforeHook({}, [log](TestContext&) { log->push_back("before2"); });
    registry.AddAfterHook({}, [log](TestContext&) { log->push_back("after1"); });
    registry.AddAfterHook({}, [log](TestContext&) { log->push_back("after2"); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Execution order
  Background:
    Given a background step
  Scenario: Ordered
    When scenario step one
    Then scenario step two
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");
    ASSERT_TRUE(result.allPassed);

    const std::vector<std::string> expected{"before1", "before2", "background", "step1", "step2", "after1", "after2"};
    EXPECT_EQ(*log, expected);
}

// ---------------------------------------------------------------------
// StepResult::location must reflect the real .feature file position (the
// second of the "two known bug fixes"), not the interpreter's own
// dispatch call site. Forced collect-failures mode (see bdd.hpp's
// impl::RunScenario()) records a StepResult - including its location -
// for every step, pass or fail, so this is checkable on an all-passing
// scenario with no death test needed.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, StepLocationsReflectRealFeatureTextLineAndColumn) {
    StepRegistry registry;
    registry.RegisterGiven("a background precondition", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I do something", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("it should work", [](TestContext&) -> bool { return true; });

    // Same hand-counted line/column scheme as
    // GherkinParser.StepLineAndColumnTrackingReflectsRealPosition in
    // test_Gherkin_Parser.cpp:
    //   line 4, col 5 -> background "Given a background precondition"
    //   line 5         -> "Scenario:" itself
    //   line 6, col 7 -> "When I do something" (6 leading spaces)
    //   line 7, col 5 -> "Then it should work" (4 leading spaces)
    constexpr std::string_view feature = R"FEATURE(
Feature: Location tracking
  Background:
    Given a background precondition
  Scenario: Single scenario
      When I do something
    Then it should work
)FEATURE";

    const std::string_view featureLabel = "features/location.feature";
    const FeatureResult result = RunFeature(feature, registry, featureLabel);

    ASSERT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    const auto& steps = result.scenarioResults[0].steps;
    ASSERT_EQ(steps.size(), 3u);

    EXPECT_EQ(steps[0].location, "features/location.feature:4:5");
    EXPECT_EQ(steps[1].location, "features/location.feature:6:7");
    EXPECT_EQ(steps[2].location, "features/location.feature:7:5");

    // None empty, and no two identical - a stubbed-out location tracker
    // (always empty, or always the same call-site string) would fail
    // this even if the exact numbers above were somehow also wrong.
    for (const auto& step : steps) {
        EXPECT_FALSE(step.location.empty());
    }
    EXPECT_NE(steps[0].location, steps[1].location);
    EXPECT_NE(steps[1].location, steps[2].location);
    EXPECT_NE(steps[0].location, steps[2].location);
}

TEST(GherkinIntegration, HookLocationsUseScenarioLineWithColumnZero) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    registry.AddBeforeHook({}, [](TestContext&) {});
    registry.AddAfterHook({}, [](TestContext&) {});

    // "Scenario:" is on line 3 here (1: "", 2: "Feature: ...", 3: "  Scenario: ...").
    constexpr std::string_view feature = R"FEATURE(
Feature: Hook locations
  Scenario: Only one
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "hooks.feature");

    ASSERT_TRUE(result.allPassed);
    const auto& steps = result.scenarioResults[0].steps;
    ASSERT_EQ(steps.size(), 3u);
    EXPECT_EQ(steps[0].stepName, "[Before] (always)");
    EXPECT_EQ(steps[0].location, "hooks.feature:3:0");
    EXPECT_EQ(steps[2].stepName, "[After] (always)");
    EXPECT_EQ(steps[2].location, "hooks.feature:3:0");
}

// ---------------------------------------------------------------------
// Bug fix #1: no double "Given a: <name>" narration line for a
// Gherkin-sourced scenario. BabyBehaveTest::Execute() would otherwise
// unconditionally print "Given a: <scenario name>" before the first real
// step (see bdd.hpp's m_suppressGivenNarration) - for a Gherkin Scenario
// whose synthetic ContextSetup does nothing, that line is pure narration
// noise on top of the Scenario's own real Given step. This must never
// appear for a Gherkin scenario, regardless of what its first step's
// keyword is.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, NoDoubleGivenNarrationArtifactForGherkinScenario) {
    StepRegistry registry;
    registry.RegisterGiven("an empty basket", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Narration
  Scenario: Starts with Given
    Given an empty basket
)FEATURE";

    SetNarrationEnabled(true);
    SetNarrationStyle(NarrationStyle::Plain);
    testing::internal::CaptureStdout();

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    const std::string capturedOut = testing::internal::GetCapturedStdout();
    SetNarrationEnabled(false);

    ASSERT_TRUE(result.allPassed);
    EXPECT_EQ(capturedOut.find("Given a: Starts with Given"), std::string::npos)
        << "synthetic Gherkin ContextSetup must not narrate its own 'Given a:' line; got:\n"
        << capturedOut;
    // The step itself must still narrate normally (Precondition uses the
    // "    With: " prefix - see bdd.hpp's detail::StepMeta<Precondition>).
    EXPECT_NE(capturedOut.find("With: an empty basket"), std::string::npos) << capturedOut;
}

// ---------------------------------------------------------------------
// Registration-time errors: pattern/callable placeholder-count mismatch,
// and an unrecognized placeholder name, both throw std::invalid_argument
// immediately from the public StepRegistry API (not deferred to
// .feature-parse-time or scenario-execution-time).
// ---------------------------------------------------------------------

TEST(GherkinIntegration, RegisteringMismatchedPlaceholderCountThrows) {
    StepRegistry registry;
    EXPECT_THROW(
        {
            registry.RegisterGiven("I have {int} apples and {int} oranges",
                                    [](TestContext&, int) -> bool { return true; });
        },
        std::invalid_argument);
}

TEST(GherkinIntegration, RegisteringUnknownPlaceholderNameThrows) {
    StepRegistry registry;
    EXPECT_THROW(
        { registry.RegisterGiven("a thing costs {money}", [](TestContext&, int) -> bool { return true; }); },
        std::invalid_argument);
}

// ---------------------------------------------------------------------
// Fail-hard paths: unmatched step, an assertion failure with forced
// collect-failures semantics still executing later steps/After hooks, and
// a malformed feature relayed all the way through the public RunFeature()
// entry point. All three call std::exit(EXIT_FAILURE) - see this file's
// header comment for why EXPECT_EXIT is required here. SetNarrationEnabled
// (true) is set right before each death-test statement so the forked
// child's own detail::PrintErrorLine() calls actually produce output to
// match against - by default ctest sets BABYBEHAVE_QUIET=1 for this binary
// (see tests/CMakeLists.txt), which would otherwise silence them exactly
// as it does for every other test in this suite.
// ---------------------------------------------------------------------

TEST(GherkinIntegrationDeathTest, UnmatchedStepFailsHardWithClearMessage) {
    SetNarrationEnabled(true);
    EXPECT_EXIT(
        {
            StepRegistry registry;
            registry.RegisterGiven("an empty basket", [](TestContext&) -> bool { return true; });
            // Deliberately no definition registered for "I addd 3 apples".
            constexpr std::string_view feature = R"FEATURE(
Feature: Typo in step text
  Scenario: Unmatched step
    Given an empty basket
    When I addd 3 apples
)FEATURE";
            RunFeature(feature, registry, "typo.feature");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "no step definition matches: 'I addd 3 apples'");
    SetNarrationEnabled(false);
}

TEST(GherkinIntegrationDeathTest, MalformedFeatureFailsHardThroughRunFeature) {
    SetNarrationEnabled(true);
    EXPECT_EXIT(
        {
            StepRegistry registry;
            constexpr std::string_view feature = R"FEATURE(
Feature: Has a Rule
  Rule: not supported
)FEATURE";
            RunFeature(feature, registry, "malformed.feature");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "'Rule:' is not supported");
    SetNarrationEnabled(false);
}

// Forced collect-failures semantics: a failing step must not stop the rest
// of the scenario. The step/hook after the failing one writes a distinct
// marker straight to std::cerr (bypassing the narration gate entirely,
// unlike detail::PrintErrorLine, so this doesn't depend on
// SetNarrationEnabled at all) - if collect-failures were NOT forced (i.e.
// if BabyBehaveTest's normal exit-on-first-failure default were used
// instead), the marker would never be reached, since the process would
// std::exit() at the first failing step already. Split into two separate
// EXPECT_EXIT statements (one per marker, checked via a plain substring
// regex) rather than one combined matcher, since EXPECT_EXIT's message
// parameter here is a POSIX-regex string (this repo doesn't link gmock,
// so ::testing::AllOf/HasSubstr aren't available) and both markers can
// land on separate stderr lines, which a single "." can't reliably be
// counted on to span.
TEST(GherkinIntegrationDeathTest, LaterStepAfterAnEarlierFailureStillRuns) {
    EXPECT_EXIT(
        {
            StepRegistry registry;
            registry.RegisterGiven("a valid card", [](TestContext&) -> bool { return true; });
            registry.RegisterWhen("I charge a negative amount", [](TestContext&) -> bool {
                return false; // fails
            });
            registry.RegisterThen("this later step still runs", [](TestContext&) -> bool {
                std::cerr << "LATER_STEP_RAN" << std::endl;
                return true;
            });
            registry.AddAfterHook({}, [](TestContext&) { std::cerr << "AFTER_HOOK_RAN" << std::endl; });

            constexpr std::string_view feature = R"FEATURE(
Feature: Forced collect-failures
  Scenario: One failure must not stop the rest
    Given a valid card
    When I charge a negative amount
    Then this later step still runs
)FEATURE";
            RunFeature(feature, registry, "collect-failures.feature");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "LATER_STEP_RAN");
}

// ---------------------------------------------------------------------
// v0.8.1: RunFeature()'s onFailureCallback extension point. A consumer-
// supplied GherkinFailureCallback that does NOT exit/throw lets RunFeature()
// return normally (instead of the death-test-only default-exit path above),
// with FeatureResult::allPassed==false and the exact message the callback
// received available for inspection. Covers both failure sources: a parse
// error (RunFeature never even reaches Scenario execution) and a failing
// Scenario (RunFeature keeps going to any later Scenarios, i.e. "collect
// Gherkin failures across the whole feature" - see impl::RunScenario).
// ---------------------------------------------------------------------

TEST(GherkinIntegration, CustomFailureCallbackForParseErrorReturnsInsteadOfExiting) {
    StepRegistry registry;
    constexpr std::string_view feature = R"FEATURE(
Feature: Has a Rule
  Rule: not supported
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) {
        collected.emplace_back(message);
    };

    const FeatureResult result = RunFeature(feature, registry, "malformed.feature", callback);

    // RunFeature returned normally (no exit) - the whole point of this test.
    EXPECT_FALSE(result.allPassed);
    EXPECT_TRUE(result.featureName.empty());
    EXPECT_TRUE(result.scenarioResults.empty());
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("'Rule:' is not supported"), std::string::npos) << collected[0];
}

TEST(GherkinIntegration, CustomFailureCallbackForScenarioFailureCollectsMessageAndKeepsGoing) {
    StepRegistry registry;
    registry.RegisterGiven("a valid card", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I charge a negative amount", [](TestContext&) -> bool { return false; });
    registry.RegisterThen("this later step still runs", [](TestContext&) -> bool { return true; });
    registry.RegisterGiven("a second scenario runs too", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Non-exiting custom callback
  Scenario: One failure must not stop the rest
    Given a valid card
    When I charge a negative amount
    Then this later step still runs

  Scenario: A later scenario in the same feature
    Given a second scenario runs too
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) {
        collected.emplace_back(message);
    };

    const FeatureResult result = RunFeature(feature, registry, "non-exiting.feature", callback);

    // The non-exiting callback let RunFeature continue past the failing
    // Scenario into the second, passing one - both are present in the result.
    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_FALSE(result.scenarioResults[0].allPassed);
    EXPECT_TRUE(result.scenarioResults[1].allPassed);

    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("BabyBehave::Gherkin: scenario 'One failure must not stop the rest' failed:"),
              std::string::npos)
        << collected[0];
    EXPECT_NE(collected[0].find("[Action] I charge a negative amount: Action failed"), std::string::npos)
        << collected[0];
    EXPECT_NE(collected[0].find("non-exiting.feature:"), std::string::npos) << collected[0];
}

TEST(GherkinIntegrationDeathTest, AfterHookStillRunsAfterAnEarlierFailure) {
    EXPECT_EXIT(
        {
            StepRegistry registry;
            registry.RegisterGiven("a valid card", [](TestContext&) -> bool { return true; });
            registry.RegisterWhen("I charge a negative amount", [](TestContext&) -> bool {
                return false; // fails
            });
            registry.RegisterThen("this later step still runs", [](TestContext&) -> bool { return true; });
            registry.AddAfterHook({}, [](TestContext&) { std::cerr << "AFTER_HOOK_RAN" << std::endl; });

            constexpr std::string_view feature = R"FEATURE(
Feature: Forced collect-failures
  Scenario: One failure must not stop the rest
    Given a valid card
    When I charge a negative amount
    Then this later step still runs
)FEATURE";
            RunFeature(feature, registry, "collect-failures.feature");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "AFTER_HOOK_RAN");
}

// ---------------------------------------------------------------------
// StepRegistry::Merge() - combining a shared "library" registry with a
// few extra, test-specific step definitions (see examples/gherkin/
// Gherkin{Bakery,Library}*.cpp for the real-world reuse pattern this
// supports: a factory function returns a populated StepRegistry by value,
// and each test either uses it directly or Merge()s in a handful of
// extra steps of its own).
// ---------------------------------------------------------------------

TEST(GherkinRegistryMerge, MergingCombinesStepDefinitionsFromBothRegistries) {
    StepRegistry shared;
    shared.RegisterGiven("a warmed-up oven", [](TestContext& ctx) -> bool {
        ctx.Set("oven_ready", true);
        return true;
    });

    StepRegistry extra;
    extra.RegisterWhen("the dough is placed inside", [](TestContext& ctx) -> bool {
        ctx.Set("baking", ctx.Get<bool>("oven_ready"));
        return true;
    });
    extra.RegisterThen("the bread should be baking", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("baking");
    });

    shared.Merge(extra);

    constexpr std::string_view feature = R"FEATURE(
Feature: Merge combines steps from both registries
  Scenario: Steps registered on either registry are all reachable after Merge
    Given a warmed-up oven
    When the dough is placed inside
    Then the bread should be baking
)FEATURE";

    const FeatureResult result = RunFeature(feature, shared, "merge-combines.feature");
    EXPECT_TRUE(result.allPassed);
}

TEST(GherkinRegistryMerge, RegistriesRemainIndependentAfterMerge) {
    StepRegistry a;
    a.RegisterGiven("a step known only to a", [](TestContext&) -> bool { return true; });

    StepRegistry b;
    b.RegisterGiven("a step known only to b", [](TestContext&) -> bool { return true; });

    a.Merge(b);

    // Mutating each registry after the Merge() call must not leak across -
    // *this and other are fully independent copies from that point on.
    a.RegisterGiven("added to a after merge", [](TestContext&) -> bool { return true; });
    b.RegisterGiven("added to b after merge", [](TestContext&) -> bool { return true; });

    EXPECT_TRUE(a.TryMatch(GherkinImpl::StepKeyword::Given, "a step known only to b").has_value());
    EXPECT_TRUE(a.TryMatch(GherkinImpl::StepKeyword::Given, "a step known only to a").has_value());
    EXPECT_TRUE(a.TryMatch(GherkinImpl::StepKeyword::Given, "added to a after merge").has_value());
    // b's post-merge addition must not have reached a.
    EXPECT_FALSE(a.TryMatch(GherkinImpl::StepKeyword::Given, "added to b after merge").has_value());

    // b never received a's steps (Merge() is one-directional: a.Merge(b)
    // only copies b's definitions into a, not the reverse).
    EXPECT_FALSE(b.TryMatch(GherkinImpl::StepKeyword::Given, "a step known only to a").has_value());
    EXPECT_TRUE(b.TryMatch(GherkinImpl::StepKeyword::Given, "a step known only to b").has_value());
    EXPECT_TRUE(b.TryMatch(GherkinImpl::StepKeyword::Given, "added to b after merge").has_value());
    // a's post-merge addition must not have reached b.
    EXPECT_FALSE(b.TryMatch(GherkinImpl::StepKeyword::Given, "added to a after merge").has_value());
}

TEST(GherkinRegistryMerge, FirstRegisteredPatternWinsOnAmbiguousOverlap) {
    StepRegistry a;
    a.RegisterGiven("an ambiguous step", [](TestContext& ctx) -> bool {
        ctx.Set("winner", std::string("a"));
        return true;
    });

    StepRegistry b;
    b.RegisterGiven("an ambiguous step", [](TestContext& ctx) -> bool {
        ctx.Set("winner", std::string("b"));
        return true;
    });

    a.Merge(b);

    // TryMatch's first-match-wins linear scan means a's own (pre-merge)
    // registration still wins over b's merged-in duplicate - the same
    // pre-existing behavior as registering a duplicate pattern directly,
    // nothing new introduced by Merge.
    std::optional<StepFunction> matched = a.TryMatch(GherkinImpl::StepKeyword::Given, "an ambiguous step");
    ASSERT_TRUE(matched.has_value());
    StepFunction stepFn = std::move(*matched);
    TestContext ctx;
    EXPECT_TRUE(stepFn(ctx));
    EXPECT_EQ(ctx.Get<std::string>("winner"), "a");
}
