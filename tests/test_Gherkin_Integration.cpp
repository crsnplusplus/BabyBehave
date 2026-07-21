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

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

// ---------------------------------------------------------------------
// Feature 7: expression-based hooks (StepRegistry::AddBeforeHookExpr/
// AddAfterHookExpr, e.g. "@smoke and not @slow") - a parallel, more
// expressive alternative to the vector-of-tags AND/subset hooks tested
// above. Registration-time parsing/error behavior is covered in
// test_Gherkin_Parser.cpp (isolated from RunFeature); these tests confirm
// the real end-to-end matching against actual Scenario tags, driven
// through RunFeature() itself.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, ExpressionBasedBeforeHookFiresOnAndOrNotCombinations) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto andNotCount = std::make_shared<int>(0);
    auto orCount = std::make_shared<int>(0);
    // AND + NOT: must fire only when @smoke is present AND @slow is absent.
    registry.AddBeforeHookExpr("@smoke and not @slow", [andNotCount](TestContext&) { ++(*andNotCount); });
    // OR: must fire when either @vip or @premium (or both) is present.
    registry.AddBeforeHookExpr("@vip or @premium", [orCount](TestContext&) { ++(*orCount); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Expression-based Before hook AND/OR/NOT combinations
  @smoke
  Scenario: A - smoke only, AND-NOT expression must fire
    Given a step

  @smoke @slow
  Scenario: B - smoke and slow, AND-NOT expression must not fire
    Given a step

  @slow
  Scenario: C - slow only, AND-NOT expression must not fire
    Given a step

  Scenario: D - untagged, neither expression fires
    Given a step

  @vip
  Scenario: E - vip only, OR expression must fire
    Given a step

  @premium
  Scenario: F - premium only, OR expression must fire
    Given a step

  @vip @premium
  Scenario: G - both vip and premium, OR expression must fire
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 7u);
    EXPECT_EQ(*andNotCount, 1) << "'@smoke and not @slow' must fire only for Scenario A";
    EXPECT_EQ(*orCount, 3) << "'@vip or @premium' must fire for Scenarios E, F, and G";
}

TEST(GherkinIntegration, ExpressionBasedAfterHookFiresOnAndOrNotCombinations) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto andNotCount = std::make_shared<int>(0);
    auto orCount = std::make_shared<int>(0);
    registry.AddAfterHookExpr("@a and not @b", [andNotCount](TestContext&) { ++(*andNotCount); });
    registry.AddAfterHookExpr("@x or @y", [orCount](TestContext&) { ++(*orCount); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Expression-based After hook AND/OR/NOT combinations
  @a
  Scenario: A - a only, AND-NOT expression must fire
    Given a step

  @a @b
  Scenario: B - a and b, AND-NOT expression must not fire
    Given a step

  Scenario: C - untagged, neither expression fires
    Given a step

  @x
  Scenario: D - x only, OR expression must fire
    Given a step

  @y
  Scenario: E - y only, OR expression must fire
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 5u);
    EXPECT_EQ(*andNotCount, 1) << "'@a and not @b' must fire only for Scenario A";
    EXPECT_EQ(*orCount, 2) << "'@x or @y' must fire for Scenarios D and E";
}

// Mixing a vector-based AddBeforeHook and an expression-based
// AddBeforeHookExpr on the SAME registry must not cross-interfere: each
// mechanism dispatches on its own hook.tags/hook.expression field (see
// impl::MatchesHookTags in bdd.hpp) and must ignore tags that are only
// meaningful to the other mechanism.
TEST(GherkinIntegration, VectorBasedAndExpressionBasedHooksCoexistWithoutCrossInterference) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto vectorCount = std::make_shared<int>(0);
    auto exprCount = std::make_shared<int>(0);
    // Vector-based (AND/subset): fires only when @needs is present. @a/@b
    // are irrelevant to this hook.
    registry.AddBeforeHook({"needs"}, [vectorCount](TestContext&) { ++(*vectorCount); });
    // Expression-based (OR): fires when @a or @b is present. @needs is
    // irrelevant to this hook.
    registry.AddBeforeHookExpr("@a or @b", [exprCount](TestContext&) { ++(*exprCount); });

    constexpr std::string_view feature = R"FEATURE(
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
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 4u);
    EXPECT_EQ(*vectorCount, 2) << "vector-based hook must fire for A and C only (both carry @needs)";
    EXPECT_EQ(*exprCount, 2) << "expression-based hook must fire for B and C only (both carry @a or @b)";
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
// impl::RunScenarioAttempt()) records a StepResult - including its location -
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
// Data Tables (Gherkin::DataTable / arity-based raw-argument dispatch): a
// step definition may declare ONE extra trailing parameter beyond its
// {int}/{float}/{string}/{word} placeholders to receive the '|'-table
// block attached to its .feature step - see StepRegistry's
// impl::StepDefinitionRawArgKind/impl::MakeStepThunk. N == P (no trailing
// parameter) is exercised throughout every other TEST in this file; these
// specifically cover N == P + 1 with a trailing DataTable.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, StepDefinitionWithOnlyATrailingDataTableParameterReceivesIt) {
    StepRegistry registry;
    registry.RegisterGiven("the following items", [](TestContext& ctx, const DataTable& table) -> bool {
        ctx.Set("rowCount", static_cast<int>(table.RowCount()));
        ctx.Set("firstName", table.Get(0, "name"));
        ctx.Set("secondQty", table.Get(1, "qty"));
        return true;
    });
    registry.RegisterThen("the table was received", [](TestContext& ctx) -> bool {
        return ctx.Get<int>("rowCount") == 2 && ctx.Get<std::string>("firstName") == "apple" &&
               ctx.Get<std::string>("secondQty") == "5";
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Data table only
  Scenario: A step with only a data table
    Given the following items
      | name  | qty |
      | apple | 3   |
      | pear  | 5   |
    Then the table was received
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

TEST(GherkinIntegration, StepDefinitionWithPlaceholdersAndATrailingDataTableReceivesBoth) {
    StepRegistry registry;
    registry.RegisterGiven("{int} items were ordered", [](TestContext& ctx, int count, const DataTable& table) -> bool {
        ctx.Set("count", count);
        ctx.Set("qty0", table.Get(0, "qty"));
        return true;
    });
    registry.RegisterThen("the order was recorded", [](TestContext& ctx) -> bool {
        return ctx.Get<int>("count") == 2 && ctx.Get<std::string>("qty0") == "3";
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Data table with placeholders
  Scenario: Captures plus a data table
    Given 2 items were ordered
      | name  | qty |
      | apple | 3   |
    Then the order was recorded
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

TEST(GherkinIntegration, RegisteringATrailingParameterThatIsNeitherDataTableNorStringThrows) {
    StepRegistry registry;
    EXPECT_THROW(
        { registry.RegisterGiven("a thing happens", [](TestContext&, int extra) -> bool { return extra == 0; }); },
        std::invalid_argument);
}

// Regression test: impl::StepDefinitionRawArgKind must reject a trailing
// parameter typed exactly `DataTable` BY VALUE (not a reference at all) -
// only exactly `const DataTable&` is accepted, matching this function's own
// error message ("must be exactly 'const DataTable&' or 'const
// std::string&'"). Before this was tightened (std::remove_cvref_t
// normalized away const-ness and reference-ness before comparing), a
// by-value `DataTable` was silently accepted at registration time - this
// specific case is deliberately used (rather than a mutable `DataTable&`,
// which the promised contract also rejects but which fails to even COMPILE
// today, independently of this check, because impl::MakeStepThunk's
// dispatch always binds the table via a const reference) so this test
// actually exercises the runtime std::invalid_argument path instead of a
// build failure.
TEST(GherkinIntegration, RegisteringATrailingByValueDataTableParameterThrows) {
    StepRegistry registry;
    EXPECT_THROW(
        { registry.RegisterGiven("a thing happens", [](TestContext&, DataTable table) -> bool { return table.RowCount() == 0; }); },
        std::invalid_argument);
}

// Regression test: a step definition with a trailing `const DataTable&`
// parameter, but whose .feature step doesn't attach any Data Table at all
// (rawArgument stays std::monostate) - impl::InvokeStepWithRawArg must
// report a clear internal-error step failure ("expects a data table but
// none is attached") rather than binding to an empty/garbage table.
TEST(GherkinIntegration, StepWithTrailingDataTableParameterButNoTableAttachedFailsClearly) {
    StepRegistry registry;
    registry.RegisterGiven("the following items", [](TestContext&, const DataTable& table) -> bool {
        return table.RowCount() == 0; // Never reached in a passing sense - see below.
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Data table parameter but no table attached
  Scenario: No '|' rows follow this step
    Given the following items
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };
    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    ASSERT_EQ(result.scenarioResults[0].steps.size(), 1u);
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("Precondition failed"), std::string::npos) << collected[0];
}

// Regression test ("narrow misuse" - see impl::InvokeStepNoRawArg's
// comment): a step definition whose ONLY parameter (besides TestContext&)
// is typed exactly `DataTable`, registered against a pattern whose
// placeholder count equals that parameter count exactly (NOT +1) - so
// impl::StepDefinitionRawArgKind computes RawArgumentKind::None (this is
// treated as an ordinary placeholder-capturing step, not a raw-argument
// one) and registration succeeds (the placeholder-count check only compares
// COUNTS, not F's actual parameter type). At invocation time this falls
// through to InvokeStepNoRawArg's defensive internal-error branch instead
// of ever calling ConvertCapture<DataTable> (which would not even compile).
TEST(GherkinIntegration, StepWhoseSoleParameterIsDataTableButRawArgKindIsNoneFailsWithInternalError) {
    StepRegistry registry;
    // Pattern has exactly 1 placeholder ({int}); F has exactly 1 parameter
    // beyond TestContext& (DataTable) - counts match, so this registers
    // successfully even though DataTable can never come from a regex capture.
    registry.RegisterGiven("{int} things happen", [](TestContext&, DataTable table) -> bool {
        return table.RowCount() == 0;
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: DataTable parameter miscounted as a placeholder
  Scenario: Placeholder count matches but the type cannot really convert
    Given 5 things happen
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };
    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    ASSERT_EQ(result.scenarioResults[0].steps.size(), 1u);
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("Precondition failed"), std::string::npos) << collected[0];
}

// ---------------------------------------------------------------------
// Doc Strings (Gherkin's '"""'-delimited raw step argument / arity-based
// raw-argument dispatch): a step definition may declare ONE extra trailing
// `const std::string&` parameter beyond its {int}/{float}/{string}/{word}
// placeholders to receive the '"""'-block attached to its .feature step -
// see StepRegistry's impl::StepDefinitionRawArgKind/impl::MakeStepThunk
// (the exact same dispatch mechanism the Data Tables section above
// exercises for DataTable; these specifically confirm the std::string arm
// of that SAME dispatch actually delivers real content end-to-end through
// the public RunFeature() entry point, not just at the parser level - see
// test_Gherkin_Parser.cpp's own Doc String coverage for parser-only checks
// (attachment, indentation stripping, error cases)).
// ---------------------------------------------------------------------

TEST(GherkinIntegration, StepDefinitionWithOnlyATrailingDocStringParameterReceivesIt) {
    StepRegistry registry;
    registry.RegisterGiven("the article body is:", [](TestContext& ctx, const std::string& body) -> bool {
        ctx.Set("body", body);
        return true;
    });
    registry.RegisterThen("the body was received", [](TestContext& ctx) -> bool {
        // Indentation-stripping (see impl::StripDocStringIndent) must have
        // already run by the time the step function sees this: the
        // opening """'s own 6-space column is stripped from every content
        // line, and the blank line in between is preserved as an empty
        // line in the joined result - the same convention
        // test_Gherkin_Parser.cpp's DocStringIsAttachedWithOpeningDelimiterIndentationStripped
        // pins down at the parser level, confirmed here end-to-end.
        return ctx.Get<std::string>("body") == "This is a multi-line\ndoc string.\n\nIt can contain blank lines too.";
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Doc string only
  Scenario: A step with only a doc string
    Given the article body is:
      """
      This is a multi-line
      doc string.

      It can contain blank lines too.
      """
    Then the body was received
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

TEST(GherkinIntegration, StepDefinitionWithAPlaceholderAndATrailingDocStringReceivesBoth) {
    StepRegistry registry;
    registry.RegisterGiven("the review for {word} is:", [](TestContext& ctx, std::string product,
                                                             const std::string& review) -> bool {
        ctx.Set("product", product);
        ctx.Set("review", review);
        return true;
    });
    registry.RegisterThen("the review was recorded", [](TestContext& ctx) -> bool {
        return ctx.Get<std::string>("product") == "widget" &&
               ctx.Get<std::string>("review") == "Works great.\nWould buy again.";
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Doc string with a placeholder
  Scenario: Captures plus a doc string
    Given the review for widget is:
      """
      Works great.
      Would buy again.
      """
    Then the review was recorded
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

// Regression test (the std::string/Doc String mirror of
// StepWithTrailingDataTableParameterButNoTableAttachedFailsClearly above): a
// step definition with a trailing `const std::string&` parameter, but whose
// .feature step doesn't attach any Doc String at all (rawArgument stays
// std::monostate) - impl::InvokeStepWithRawArg must report a clear
// internal-error step failure ("expects a doc string but none is attached").
TEST(GherkinIntegration, StepWithTrailingDocStringParameterButNoDocStringAttachedFailsClearly) {
    StepRegistry registry;
    registry.RegisterGiven("the article body is:", [](TestContext&, const std::string& body) -> bool {
        return body.empty();
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Doc string parameter but no doc string attached
  Scenario: No '"""' block follows this step
    Given the article body is:
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };
    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    ASSERT_EQ(result.scenarioResults[0].steps.size(), 1u);
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("Precondition failed"), std::string::npos) << collected[0];
}

// Regression test: impl::ConvertCapture<T>'s `if constexpr
// (std::is_reference_v<T>)` reference-return branch is not exclusive to a
// trailing Doc String raw argument (which never even goes through
// ConvertCapture - see InvokeWithCapturesAndRaw) - it is reached by ANY
// ordinary {string}/{word} PLACEHOLDER capture parameter declared
// `const std::string&` instead of `std::string` by value, exercised here
// with no trailing raw argument at all (rawArgKind == None).
TEST(GherkinIntegration, PlaceholderCaptureDeclaredAsConstStringReferenceIsBoundCorrectly) {
    StepRegistry registry;
    registry.RegisterGiven("the item is {string}", [](TestContext& ctx, const std::string& item) -> bool {
        ctx.Set("item", item);
        return true;
    });
    registry.RegisterThen("it should be recorded", [](TestContext& ctx) -> bool {
        return ctx.Get<std::string>("item") == "a widget";
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Reference-typed placeholder capture
  Scenario: A {string} placeholder bound to const std::string&
    Given the item is "a widget"
    Then it should be recorded
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
}

// Critical regression coverage: every existing plain (no-DataTable) step
// registration/match keeps working, both through the public RunFeature()
// entry point (every other TEST in this file already exercises that) and
// directly against the OLD 2-arg StepRegistry::TryMatch(keyword, text)
// overload, which now forwards to the new 3-arg one - see StepRegistry.
TEST(GherkinIntegration, PlainStepRegistrationAndTwoArgTryMatchStillWorkUnaffected) {
    StepRegistry registry;
    registry.RegisterGiven("an empty crate", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I add {int} apples", [](TestContext& ctx, int count) -> bool {
        ctx.Set("count", count);
        return true;
    });

    const auto matchedGiven = registry.TryMatch(GherkinImpl::StepKeyword::Given, "an empty crate");
    ASSERT_TRUE(matchedGiven.has_value());

    auto matchedWhen = registry.TryMatch(GherkinImpl::StepKeyword::When, "I add 3 apples");
    ASSERT_TRUE(matchedWhen.has_value());
    TestContext ctx;
    StepFunction whenFn = std::move(*matchedWhen);
    EXPECT_TRUE(whenFn(ctx));
    EXPECT_EQ(ctx.Get<int>("count"), 3);

    const auto noMatch = registry.TryMatch(GherkinImpl::StepKeyword::Then, "nonexistent step");
    EXPECT_FALSE(noMatch.has_value());
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
// Gherkin failures across the whole feature" - see impl::RunScenarioWithRetries).
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

// Feature 7: Merge() must carry over expression-based hooks too, not just
// vector-tag-based ones - StepRegistry::Merge() copies whole impl::Hook
// vectors/objects, and impl::Hook's new `expression` field is just an
// additional member on that same struct, so this should already work
// without any Merge()-specific code change; verified here with a real,
// end-to-end RunFeature() rather than just assumed.
TEST(GherkinRegistryMerge, MergeCarriesOverExpressionBasedHooks) {
    StepRegistry shared;
    shared.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    auto count = std::make_shared<int>(0);
    StepRegistry extra;
    extra.AddBeforeHookExpr("@smoke and not @slow", [count](TestContext&) { ++(*count); });

    shared.Merge(extra);

    ASSERT_EQ(shared.BeforeHooks().size(), 1u);
    EXPECT_TRUE(shared.BeforeHooks().front().expression.has_value())
        << "the merged-in hook's expression AST must survive Merge()";

    constexpr std::string_view feature = R"FEATURE(
Feature: Merge carries over expression-based hooks
  @smoke
  Scenario: A - smoke only, must fire the merged-in expression hook
    Given a step

  @smoke @slow
  Scenario: B - smoke and slow, must not fire (not @slow fails)
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, shared, "merge-expression-hooks.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_EQ(*count, 1) << "merged-in expression hook must fire for Scenario A only";
}

// ---------------------------------------------------------------------
// Scenario Outline/Examples end-to-end: each Examples data row runs as its
// own, fully independent Scenario (RunFeature/RunScenario need zero
// awareness of "outline-ness" - impl::ExpandScenarioOutlines already turned
// each row into an ordinary ParsedScenario before RunFeature ever sees it).
// ---------------------------------------------------------------------

TEST(GherkinIntegration, ScenarioOutlineExpandsAndRunsEachExamplesRowIndependently) {
    StepRegistry registry;
    registry.RegisterGiven("an empty crate", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I add {int} {word}", [](TestContext& ctx, int count, std::string item) -> bool {
        ctx.Set("count", count);
        ctx.Set("item", item);
        return true;
    });
    registry.RegisterThen("the crate holds {int} {word}", [](TestContext& ctx, int count, std::string item) -> bool {
        return ctx.Get<int>("count") == count && ctx.Get<std::string>("item") == item;
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Basket
  Scenario Outline: Adding <count> <item>
    Given an empty crate
    When I add <count> <item>
    Then the crate holds <count> <item>

  Examples:
    | count | item   |
    | 1     | apple  |
    | 2     | orange |
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_EQ(result.scenarioResults[0].testName, "Adding <count> <item> (Examples row 1)");
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    EXPECT_EQ(result.scenarioResults[1].testName, "Adding <count> <item> (Examples row 2)");
    EXPECT_TRUE(result.scenarioResults[1].allPassed);
}

TEST(GherkinIntegration, EachExpandedExamplesRowGetsItsOwnBackgroundAndHookExecution) {
    auto log = std::make_shared<std::vector<std::string>>();

    StepRegistry registry;
    registry.RegisterGiven("a background step", [log](TestContext&) -> bool {
        log->push_back("background");
        return true;
    });
    registry.RegisterWhen("I process {word}", [log](TestContext&, std::string item) -> bool {
        log->push_back("step:" + item);
        return true;
    });
    registry.AddBeforeHook({}, [log](TestContext&) { log->push_back("before"); });
    registry.AddAfterHook({}, [log](TestContext&) { log->push_back("after"); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Outline hooks
  Background:
    Given a background step

  Scenario Outline: Processing <item>
    When I process <item>

  Examples:
    | item  |
    | alpha |
    | beta  |
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");
    ASSERT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);

    // Each row gets its own Before hook, Background, and After hook - not
    // one Background/hook pair shared/hoisted across every expanded row.
    const std::vector<std::string> expected{
        "before", "background", "step:alpha", "after", "before", "background", "step:beta", "after",
    };
    EXPECT_EQ(*log, expected);
}

TEST(GherkinIntegrationDeathTest, OutlinePlaceholderWithNoMatchingColumnSurfacesAsUnmatchedStepNotACrash) {
    SetNarrationEnabled(true);
    EXPECT_EXIT(
        {
            StepRegistry registry;
            registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });
            // Deliberately no step registered matching the literal text
            // "I use <missing>" - "<missing>" has no matching Examples
            // column ("present" is the only declared column), so it is
            // left unsubstituted per spec and must surface as an ordinary
            // "no step definition matches" failure, not a crash/exception.
            constexpr std::string_view feature = R"FEATURE(
Feature: Unmatched column
  Scenario Outline: Uses missing column
    Given a step
    When I use <missing>

  Examples:
    | present |
    | value   |
)FEATURE";
            RunFeature(feature, registry, "missing-column.feature");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "no step definition matches: 'I use <missing>'");
    SetNarrationEnabled(false);
}

// ---------------------------------------------------------------------
// @timeout annotations. Reuses the plain @token tag grammar (no parser
// change at all - see test_Gherkin_Parser.cpp's GherkinTimeoutPolicy suite
// for impl::ParseScenarioExecutionPolicy tested directly/in isolation).
// These tests cover end-to-end enforcement through the public RunFeature()
// entry point: a step that actually blocks past the deadline, the NEXT
// step failing with a clear message, After hooks still running to
// completion, and a malformed @timeout annotation failing fast before any
// step runs. All use the non-exiting custom GherkinFailureCallback (see
// the v0.8.1 section above) rather than EXPECT_EXIT, since every failure
// here is inspected via FeatureResult/TestResult, not stderr text.
// ---------------------------------------------------------------------

TEST(GherkinIntegration, ScenarioWithNoTimeoutTagIsCompletelyUnaffectedRegressionTest) {
    // The most important regression test in this section: a Scenario with
    // NO @timeout tag at all must behave exactly as it did before this
    // feature existed - no deadline is ever constructed, and
    // AddParsedStepToTest never even calls WrapWithDeadlineCheck (deadline
    // stays nullptr throughout impl::RunScenarioAttempt).
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I do something quick", [](TestContext&) -> bool { return true; });
    registry.RegisterThen("it succeeds", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: No timeout at all
  Scenario: Ordinary scenario
    Given a valid setup
    When I do something quick
    Then it succeeds
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "no-timeout.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    ASSERT_EQ(result.scenarioResults[0].steps.size(), 3u);
    for (const auto& step : result.scenarioResults[0].steps) {
        EXPECT_TRUE(step.passed);
    }
}

TEST(GherkinIntegration, StepAfterDeadlinePassesFailsWithClearTimeoutMessageAndNeverRunsForReal) {
    int thenStepRealBodyRunCount = 0;

    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I block past the deadline", [](TestContext&) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        return true;
    });
    registry.RegisterThen("this step must not really run", [&thenStepRealBodyRunCount](TestContext&) -> bool {
        ++thenStepRealBodyRunCount;
        return true;
    });

    constexpr std::string_view feature = R"FEATURE(
@timeout:30ms
Feature: Timeout enforcement
  Scenario: Blocks past its deadline
    Given a valid setup
    When I block past the deadline
    Then this step must not really run
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };

    const FeatureResult result = RunFeature(feature, registry, "timeout-exceeded.feature", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    const TestResult& scenarioResult = result.scenarioResults[0];
    EXPECT_FALSE(scenarioResult.allPassed);

    // Cooperative, INTER-STEP checking only: the blocking When step itself
    // still ran to completion (the sleep_for above is real) - only the NEXT
    // step, checked at its own start, observes the expired deadline.
    ASSERT_EQ(scenarioResult.steps.size(), 3u);
    EXPECT_TRUE(scenarioResult.steps[0].passed);
    EXPECT_TRUE(scenarioResult.steps[1].passed);
    EXPECT_FALSE(scenarioResult.steps[2].passed);
    EXPECT_NE(scenarioResult.steps[2].message.find("exceeded @timeout deadline"), std::string::npos)
        << scenarioResult.steps[2].message;

    // WrapWithDeadlineCheck throws BEFORE invoking the real step body.
    EXPECT_EQ(thenStepRealBodyRunCount, 0);

    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("exceeded @timeout deadline"), std::string::npos) << collected[0];
}

TEST(GherkinIntegration, AfterHookStillRunsToCompletionWhenAMidScenarioTimeoutOccurred) {
    bool afterHookRan = false;

    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I block past the deadline", [](TestContext&) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        return true;
    });
    registry.RegisterThen("this step must not really run", [](TestContext&) -> bool { return true; });
    registry.AddAfterHook({}, [&afterHookRan](TestContext&) { afterHookRan = true; });

    constexpr std::string_view feature = R"FEATURE(
@timeout:30ms
Feature: Timeout enforcement with After hook
  Scenario: Blocks past its deadline
    Given a valid setup
    When I block past the deadline
    Then this step must not really run
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };

    const FeatureResult result = RunFeature(feature, registry, "timeout-after-hook.feature", callback);

    EXPECT_FALSE(result.allPassed);
    EXPECT_TRUE(afterHookRan);
}

TEST(GherkinIntegration, MalformedTimeoutAnnotationFailsImmediatelyWithZeroStepsExecuted) {
    int givenStepRunCount = 0;

    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [&givenStepRunCount](TestContext&) -> bool {
        ++givenStepRunCount;
        return true;
    });
    registry.AddBeforeHook({}, [&givenStepRunCount](TestContext&) { ++givenStepRunCount; });
    registry.AddAfterHook({}, [&givenStepRunCount](TestContext&) { ++givenStepRunCount; });

    // @timeout:0s is a rejected, malformed value (zero is not allowed) -
    // must fail fast before any Before hook/Background/step/After hook runs.
    constexpr std::string_view feature = R"FEATURE(
Feature: Malformed timeout
  @timeout:0s
  Scenario: Has a bogus annotation
    Given a valid setup
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };

    const FeatureResult result = RunFeature(feature, registry, "malformed-timeout.feature", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_FALSE(result.scenarioResults[0].allPassed);

    // Nothing was set up: no Before hook, no step, no After hook ran.
    EXPECT_EQ(givenStepRunCount, 0);

    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("malformed-timeout.feature"), std::string::npos) << collected[0];
}

// Regression coverage: a Scenario carrying BOTH a tag-matched Before hook
// AND an active (non-tripping) @timeout - impl::RunScenarioAttempt must
// wrap the Before hook itself with the same deadline check every
// Background/Scenario step gets (see its own comment: "every Before
// hook/Background step/Scenario step ... is wrapped with
// WrapWithDeadlineCheck"), not just the ordinary steps. A generous timeout
// (comfortably longer than the hook/steps take) proves the wrapping itself
// doesn't change passing behavior.
TEST(GherkinIntegration, BeforeHookIsWrappedWithDeadlineCheckWhenATimeoutIsActive) {
    StepRegistry registry;
    bool hookRan = false;
    registry.AddBeforeHook({"vip"}, [&hookRan](TestContext&) { hookRan = true; });
    registry.RegisterGiven("a quick step", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
@timeout:2s
Feature: Before hook under an active timeout
  @vip
  Scenario: Before hook plus a generous timeout
    Given a quick step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "test_Gherkin_Integration.cpp");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(hookRan);
}

// ---------------------------------------------------------------------
// enableParallelScenarios (v0.9.0 Phase 1): RunFeature()'s new, additive,
// defaulted-false 5th parameter. When true, every Scenario is dispatched to
// its own std::async(std::launch::async, ...) task (see bdd.hpp's
// RunFeature() doc comment for the full design rationale/safety argument -
// each Scenario gets a brand-new BabyBehaveTest/TestContext with no shared
// mutable state, mirroring examples/gherkin/GherkinLibraryConcurrentLending.cpp's
// existing hand-rolled concurrent-dispatch precedent).
//
// enableParallelScenarios==false (the default, omitted entirely by every
// OTHER test in this file) is byte-identical to pre-Phase-1 behavior - that
// claim isn't just asserted here, it's what the entire rest of this test
// suite already exercises: every existing test above calls RunFeature() with
// its old 2-4 argument shape and must keep passing completely unchanged.
//
// Not tested here (deliberately): using the DEFAULT onFailure
// (impl::DefaultGherkinFailureAction, which calls std::exit()) together with
// enableParallelScenarios==true. That combination is documented as unsafe
// (see RunFeature()'s doc comment in bdd.hpp) precisely because std::exit()
// racing against still-running sibling scenario threads is undefined,
// unpredictable behavior - unlike the malformed-@timeout WILL_FAIL-style
// death test elsewhere in this repo, there is no way to exercise this
// combination from a gtest process without risking a hang/crash/flaky exit
// in CI. Every test below therefore installs its own non-exiting
// GherkinFailureCallback before passing enableParallelScenarios=true, per
// that same documented requirement.
// ---------------------------------------------------------------------

TEST(GherkinParallelExecution, ParallelRunPreservesDeclarationOrderRegardlessOfCompletionTiming) {
    StepRegistry registry;
    registry.RegisterGiven("nothing in particular", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I wait {int} ms", [](TestContext&, int ms) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return true;
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    // Scenario B (index 1, deliberately NOT last) sleeps far longer than its
    // siblings - if scenarioResults were ever ordered by completion time
    // instead of declaration order, B would land last instead of at index 1
    // and this test would fail.
    constexpr std::string_view feature = R"FEATURE(
Feature: Parallel declaration order
  Scenario: A - fast
    Given nothing in particular
    When I wait 0 ms
    Then it completes

  Scenario: B - deliberately slow
    Given nothing in particular
    When I wait 120 ms
    Then it completes

  Scenario: C - fast
    Given nothing in particular
    When I wait 0 ms
    Then it completes

  Scenario: D - fast
    Given nothing in particular
    When I wait 0 ms
    Then it completes
)FEATURE";

    // Every Scenario here passes, so onFailure is never actually invoked -
    // but a non-exiting callback that fails the test if it IS invoked is
    // installed anyway, per the documented requirement to never pair the
    // exiting default with enableParallelScenarios=true.
    const GherkinFailureCallback failIfInvoked = [](std::string_view message) {
        ADD_FAILURE() << "onFailure must not be invoked for an all-passing parallel Feature: " << message;
    };

    const FeatureResult result =
        RunFeature(feature, registry, "parallel-order.feature", failIfInvoked, /*enableParallelScenarios=*/true);

    ASSERT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 4u);
    EXPECT_EQ(result.scenarioResults[0].testName, "A - fast");
    EXPECT_EQ(result.scenarioResults[1].testName, "B - deliberately slow");
    EXPECT_EQ(result.scenarioResults[2].testName, "C - fast");
    EXPECT_EQ(result.scenarioResults[3].testName, "D - fast");
    for (const auto& scenarioResult : result.scenarioResults) {
        EXPECT_TRUE(scenarioResult.allPassed);
    }
}

TEST(GherkinParallelExecution, ParallelModeCollectsFailuresFromConcurrentlyFailingScenariosThreadSafely) {
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I wait {int} ms then pass", [](TestContext&, int ms) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return true;
    });
    registry.RegisterWhen("I wait {int} ms then fail", [](TestContext&, int ms) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return false;
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    // Two of the four Scenarios fail, with overlapping sleep windows so
    // their onFailure invocations genuinely race against each other rather
    // than happening to be serialized by luck.
    constexpr std::string_view feature = R"FEATURE(
Feature: Concurrent failures
  Scenario: Passing one
    Given a valid setup
    When I wait 10 ms then pass
    Then it completes

  Scenario: Failing one
    Given a valid setup
    When I wait 40 ms then fail
    Then it completes

  Scenario: Passing two
    Given a valid setup
    When I wait 20 ms then pass
    Then it completes

  Scenario: Failing two
    Given a valid setup
    When I wait 30 ms then fail
    Then it completes
)FEATURE";

    // Thread-safe by construction: an atomic counter (lock-free) plus a
    // mutex-guarded vector, exactly the pattern already used by
    // examples/gherkin/GherkinLibraryConcurrentLending.cpp's own
    // threadSafeCollectFailures callback. impl::InvokeOnFailure (bdd.hpp)
    // additionally serializes every onFailure call with its own internal
    // mutex as defense-in-depth, but this callback does not rely on that -
    // it is safe to call concurrently on its own merits too.
    std::atomic<int> invocationCount{0};
    std::mutex messagesMutex;
    std::vector<std::string> messages;
    const GherkinFailureCallback collectFailuresThreadSafely = [&](std::string_view message) {
        invocationCount.fetch_add(1, std::memory_order_relaxed);
        const std::lock_guard<std::mutex> lock(messagesMutex);
        messages.emplace_back(message);
    };

    const FeatureResult result = RunFeature(feature, registry, "parallel-failures.feature",
                                              collectFailuresThreadSafely, /*enableParallelScenarios=*/true);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 4u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    EXPECT_FALSE(result.scenarioResults[1].allPassed);
    EXPECT_TRUE(result.scenarioResults[2].allPassed);
    EXPECT_FALSE(result.scenarioResults[3].allPassed);

    // Exactly once per failing Scenario - no missed/duplicated invocations
    // from a data race in the counter/collector itself.
    EXPECT_EQ(invocationCount.load(), 2);
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_NE(messages[0].find("Failing"), std::string::npos) << messages[0];
    EXPECT_NE(messages[1].find("Failing"), std::string::npos) << messages[1];
}

// ---------------------------------------------------------------------
// @retry:N annotations (Feature 6, retry/flaky annotations). Reuses the
// plain @token tag grammar (no parser change at all - see
// test_Gherkin_Parser.cpp's GherkinRetryPolicy suite for
// impl::ParseScenarioExecutionPolicy/impl::ParseRetryValue tested directly/
// in isolation). These tests cover end-to-end retry behavior through the
// public RunFeature() entry point: a step that fails on its first N-1
// attempts and succeeds on the Nth, confirming onFailure is NEVER invoked
// for the superseded intermediate failures; a step that fails on EVERY
// attempt, confirming onFailure fires EXACTLY once with the final attempt's
// failure info; Before/After hooks re-running on every attempt (not just
// once); @timeout+@retry interaction (each attempt gets its own fresh
// deadline); a malformed @retry annotation failing fast; and
// enableParallelScenarios=true + @retry working correctly together. All use
// the non-exiting custom GherkinFailureCallback (see the @timeout section
// above) rather than EXPECT_EXIT, since every failure here is inspected via
// FeatureResult/TestResult, not stderr text.
// ---------------------------------------------------------------------

TEST(GherkinRetry, ScenarioWithNoRetryTagIsCompletelyUnaffectedRegressionTest) {
    // The most important regression test in this section: a Scenario with
    // NO @retry tag at all must behave exactly as it did before this
    // feature existed - maxAttempts stays at its default of 1, so
    // RunScenarioWithRetries's loop runs impl::RunScenarioAttempt exactly
    // once, byte-identical to the old impl::RunScenario's single-shot body.
    int runCount = 0;
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I run once", [&runCount](TestContext&) -> bool {
        ++runCount;
        return true;
    });
    registry.RegisterThen("it succeeds", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: No retry at all
  Scenario: Ordinary scenario
    Given a valid setup
    When I run once
    Then it succeeds
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "no-retry.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    EXPECT_EQ(runCount, 1);
}

TEST(GherkinRetry, ScenarioFailingTwiceThenSucceedingIsReportedPassedAndOnFailureNeverInvoked) {
    int attemptCount = 0;
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I fail twice then succeed", [&attemptCount](TestContext&) -> bool {
        ++attemptCount;
        return attemptCount >= 3; // fails on attempt 1 and 2, succeeds on attempt 3.
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
@retry:3
Feature: Eventually succeeds
  Scenario: Flaky but bounded
    Given a valid setup
    When I fail twice then succeed
    Then it completes
)FEATURE";

    const GherkinFailureCallback failIfInvoked = [](std::string_view message) {
        ADD_FAILURE() << "onFailure must not be invoked for a Scenario that ultimately succeeds via retry: "
                       << message;
    };

    const FeatureResult result = RunFeature(feature, registry, "retry-eventual-success.feature", failIfInvoked);

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    // Exactly 3 attempts were made: 2 failed (silently) then the 3rd
    // succeeded and stopped the retry loop - no 4th attempt.
    EXPECT_EQ(attemptCount, 3);
}

TEST(GherkinRetry, ScenarioFailingEveryAttemptInvokesOnFailureExactlyOnceWithFinalAttemptInfo) {
    int attemptCount = 0;
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I always fail", [&attemptCount](TestContext&) -> bool {
        ++attemptCount;
        return false;
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
@retry:3
Feature: Never succeeds
  Scenario: Permanently broken
    Given a valid setup
    When I always fail
    Then it completes
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };

    const FeatureResult result = RunFeature(feature, registry, "retry-always-fails.feature", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_FALSE(result.scenarioResults[0].allPassed);

    // All 3 attempts ran (no more, no fewer): maxAttempts exhausted.
    EXPECT_EQ(attemptCount, 3);

    // onFailure invoked EXACTLY once for the whole Scenario, not once per
    // failed attempt - the 2 earlier (superseded) failures are silent.
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("Permanently broken"), std::string::npos) << collected[0];
}

TEST(GherkinRetry, BeforeAndAfterHooksRerunOnEveryAttemptNotJustOnce) {
    // Design decision under test: every retry attempt is a full,
    // independent re-run of Before hooks -> Background -> Steps -> After
    // hooks (see impl::RunScenarioWithRetries's doc comment in bdd.hpp for
    // the full justification). A scenario that fails on its first 2
    // attempts and succeeds on the 3rd must show BOTH hooks having run
    // exactly 3 times each - once per attempt, including the 2 discarded
    // ones - not just once overall.
    int beforeHookCount = 0;
    int afterHookCount = 0;
    int attemptCount = 0;

    StepRegistry registry;
    registry.AddBeforeHook({}, [&beforeHookCount](TestContext&) { ++beforeHookCount; });
    registry.AddAfterHook({}, [&afterHookCount](TestContext&) { ++afterHookCount; });
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I fail twice then succeed", [&attemptCount](TestContext&) -> bool {
        ++attemptCount;
        return attemptCount >= 3;
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
@retry:3
Feature: Hooks rerun per attempt
  Scenario: Flaky but bounded
    Given a valid setup
    When I fail twice then succeed
    Then it completes
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "retry-hooks-rerun.feature");

    EXPECT_TRUE(result.allPassed);
    EXPECT_EQ(attemptCount, 3);
    EXPECT_EQ(beforeHookCount, 3);
    EXPECT_EQ(afterHookCount, 3);
}

TEST(GherkinRetry, TimeoutAndRetryGiveEachAttemptItsOwnIndependentFreshDeadline) {
    // Proves the "each attempt gets its own fresh deadline" contract: the
    // FIRST attempt sleeps long enough to blow its @timeout budget (making
    // the Then step throw "exceeded @timeout deadline" and fail that
    // attempt); the SECOND attempt does not sleep at all. If a ScenarioDeadline
    // were incorrectly reused/carried over across retry attempts (instead of
    // being freshly constructed per attempt - see impl::RunScenarioAttempt),
    // the second attempt's deadline would already read as expired the
    // instant it started (its `start` time would predate the first
    // attempt's 80ms sleep), and this test would fail with the Scenario
    // still reported as failed even though attempt 2's steps are trivially
    // fast.
    int attemptCount = 0;
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("I block only on the first attempt", [&attemptCount](TestContext&) -> bool {
        ++attemptCount;
        if (attemptCount == 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
        return true;
    });
    registry.RegisterThen("it completes within its own fresh deadline", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
@timeout:30ms
@retry:2
Feature: Timeout and retry interaction
  Scenario: First attempt times out, second attempt has a fresh budget
    Given a valid setup
    When I block only on the first attempt
    Then it completes within its own fresh deadline
)FEATURE";

    const GherkinFailureCallback failIfInvoked = [](std::string_view message) {
        ADD_FAILURE() << "onFailure must not be invoked once the second attempt's fresh deadline lets it pass: "
                       << message;
    };

    const FeatureResult result =
        RunFeature(feature, registry, "retry-timeout-fresh-deadline.feature", failIfInvoked);

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    EXPECT_EQ(attemptCount, 2);
}

TEST(GherkinRetry, MalformedRetryAnnotationFailsImmediatelyWithZeroStepsExecuted) {
    int givenStepRunCount = 0;

    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [&givenStepRunCount](TestContext&) -> bool {
        ++givenStepRunCount;
        return true;
    });
    registry.AddBeforeHook({}, [&givenStepRunCount](TestContext&) { ++givenStepRunCount; });
    registry.AddAfterHook({}, [&givenStepRunCount](TestContext&) { ++givenStepRunCount; });

    // @retry:0 is a rejected, malformed value (zero total attempts is not
    // allowed) - must fail fast before any Before hook/Background/step/
    // After hook runs, exactly like a malformed @timeout does.
    constexpr std::string_view feature = R"FEATURE(
Feature: Malformed retry
  @retry:0
  Scenario: Has a bogus annotation
    Given a valid setup
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback callback = [&collected](std::string_view message) { collected.emplace_back(message); };

    const FeatureResult result = RunFeature(feature, registry, "malformed-retry.feature", callback);

    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 1u);
    EXPECT_FALSE(result.scenarioResults[0].allPassed);

    // Nothing was set up: no Before hook, no step, no After hook ran - a
    // malformed @retry is a static defect, not a flaky runtime failure, so
    // it is never itself "retried".
    EXPECT_EQ(givenStepRunCount, 0);

    ASSERT_EQ(collected.size(), 1u);
    EXPECT_NE(collected[0].find("malformed-retry.feature"), std::string::npos) << collected[0];
}

TEST(GherkinParallelExecution, ParallelRetrySucceedsCorrectlyUnderConcurrentDispatch) {
    // Two Scenarios, each with its own @retry:3, dispatched concurrently
    // (enableParallelScenarios=true): Scenario A fails its first 2 attempts
    // then succeeds on the 3rd; Scenario B fails all 3 attempts. Each
    // Scenario's retry loop is private to its own std::async task (see
    // impl::RunScenarioWithRetries's doc comment in bdd.hpp), so this must
    // produce the exact same per-Scenario outcome as the equivalent serial
    // tests above, with no cross-scenario interference.
    int counterA = 0;
    int counterB = 0;
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("scenario A fails twice then succeeds", [&counterA](TestContext&) -> bool {
        ++counterA;
        return counterA >= 3;
    });
    registry.RegisterWhen("scenario B always fails", [&counterB](TestContext&) -> bool {
        ++counterB;
        return false;
    });
    registry.RegisterThen("it completes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Parallel retry
  @retry:3
  Scenario: A - eventually succeeds
    Given a valid setup
    When scenario A fails twice then succeeds
    Then it completes

  @retry:3
  Scenario: B - always fails
    Given a valid setup
    When scenario B always fails
    Then it completes
)FEATURE";

    std::atomic<int> invocationCount{0};
    std::mutex messagesMutex;
    std::vector<std::string> messages;
    const GherkinFailureCallback collectFailuresThreadSafely = [&](std::string_view message) {
        invocationCount.fetch_add(1, std::memory_order_relaxed);
        const std::lock_guard<std::mutex> lock(messagesMutex);
        messages.emplace_back(message);
    };

    const FeatureResult result = RunFeature(feature, registry, "parallel-retry.feature",
                                              collectFailuresThreadSafely, /*enableParallelScenarios=*/true);

    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_EQ(result.scenarioResults[0].testName, "A - eventually succeeds");
    EXPECT_EQ(result.scenarioResults[1].testName, "B - always fails");
    EXPECT_TRUE(result.scenarioResults[0].allPassed);
    EXPECT_FALSE(result.scenarioResults[1].allPassed);
    EXPECT_FALSE(result.allPassed);

    EXPECT_EQ(counterA, 3);
    EXPECT_EQ(counterB, 3);

    // Scenario A's two intermediate failed attempts never reach onFailure;
    // Scenario B's failure is reported exactly once (its final attempt).
    EXPECT_EQ(invocationCount.load(), 1);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_NE(messages[0].find("B - always fails"), std::string::npos) << messages[0];
}

// ---------------------------------------------------------------------
// Feature 8: Suite-level Before-ALL/After-ALL hooks
// (StepRegistry::AddBeforeAllHook/AddAfterAllHook). Unlike AddBeforeHook/
// AddAfterHook (tag-filtered, run once per matching Scenario), these run
// exactly ONCE per RunFeature() call regardless of how many Scenarios the
// Feature contains: once before ANY Scenario starts, once after EVERY
// Scenario has finished. These tests cover: run-exactly-once semantics
// (not once-per-Scenario); multi-hook registration-order execution;
// After-ALL still running when a Scenario fails, but ONLY under a
// non-exiting onFailure (see AddBeforeAllHook's doc comment in bdd.hpp for
// why the DEFAULT exiting onFailure means After-ALL never runs at all -
// that specific asymmetry is a direct, untestable-without-gmock
// consequence of std::exit() terminating the process before control ever
// returns to RunFeature's After-ALL loop, mirrored here only as a comment,
// exactly like the equivalent enableParallelScenarios/onFailure caveat
// documented in bdd.hpp is not covered by a dedicated "never runs" unit
// test either); correct behavior under enableParallelScenarios=true
// (Before-ALL fully completes before any Scenario step runs, After-ALL only
// starts once every Scenario - including retries - has finished); and zero
// interaction with per-Scenario AddBeforeHook/AddAfterHook/
// AddBeforeHookExpr/AddAfterHookExpr firing at their own, unaffected scope.
// ---------------------------------------------------------------------

TEST(GherkinSuiteHooks, BeforeAllHookRunsExactlyOnceRegardlessOfScenarioCount) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    int beforeAllCount = 0;
    registry.AddBeforeAllHook([&beforeAllCount]() { ++beforeAllCount; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Suite-level Before-ALL runs once
  Scenario: First
    Given a step

  Scenario: Second
    Given a step

  Scenario: Third
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "suite-before-all.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 3u);
    // Exactly once, NOT once per Scenario (which would be 3) - the crux of
    // Feature 8's "Suite-level" semantic.
    EXPECT_EQ(beforeAllCount, 1);
}

TEST(GherkinSuiteHooks, AfterAllHookRunsExactlyOnceAfterAllScenariosFinish) {
    StepRegistry registry;
    std::vector<std::string> log;

    registry.RegisterGiven("scenario {word} runs", [&log](TestContext&, std::string name) -> bool {
        log.push_back("scenario:" + name);
        return true;
    });

    int afterAllCount = 0;
    registry.AddAfterAllHook([&afterAllCount, &log]() {
        ++afterAllCount;
        log.emplace_back("afterAll");
    });

    constexpr std::string_view feature = R"FEATURE(
Feature: Suite-level After-ALL runs once
  Scenario: A
    Given scenario A runs

  Scenario: B
    Given scenario B runs
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "suite-after-all.feature");

    EXPECT_TRUE(result.allPassed);
    // Exactly once, NOT once per Scenario (which would be 2).
    EXPECT_EQ(afterAllCount, 1);
    // And strictly after BOTH Scenarios' own steps, not interleaved.
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "scenario:A");
    EXPECT_EQ(log[1], "scenario:B");
    EXPECT_EQ(log[2], "afterAll");
}

TEST(GherkinSuiteHooks, MultipleBeforeAllAndAfterAllHooksRunInRegistrationOrder) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    std::vector<std::string> log;
    // Mirrors AddBeforeHook/AddAfterHook's own documented convention: Before
    // hooks run in registration order, and so do After hooks (not reversed).
    registry.AddBeforeAllHook([&log]() { log.emplace_back("before1"); });
    registry.AddBeforeAllHook([&log]() { log.emplace_back("before2"); });
    registry.AddBeforeAllHook([&log]() { log.emplace_back("before3"); });
    registry.AddAfterAllHook([&log]() { log.emplace_back("after1"); });
    registry.AddAfterAllHook([&log]() { log.emplace_back("after2"); });
    registry.AddAfterAllHook([&log]() { log.emplace_back("after3"); });

    constexpr std::string_view feature = R"FEATURE(
Feature: Multiple suite hooks
  Scenario: Only one
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "suite-multi.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(log.size(), 6u);
    EXPECT_EQ(log[0], "before1");
    EXPECT_EQ(log[1], "before2");
    EXPECT_EQ(log[2], "before3");
    EXPECT_EQ(log[3], "after1");
    EXPECT_EQ(log[4], "after2");
    EXPECT_EQ(log[5], "after3");
}

TEST(GherkinSuiteHooks, AfterAllHookStillRunsWhenAScenarioFailsUnderNonExitingOnFailure) {
    StepRegistry registry;
    registry.RegisterGiven("a valid setup", [](TestContext&) -> bool { return true; });
    registry.RegisterWhen("it fails", [](TestContext&) -> bool { return false; });
    registry.RegisterThen("it passes", [](TestContext&) -> bool { return true; });

    int beforeAllCount = 0;
    int afterAllCount = 0;
    registry.AddBeforeAllHook([&beforeAllCount]() { ++beforeAllCount; });
    registry.AddAfterAllHook([&afterAllCount]() { ++afterAllCount; });

    constexpr std::string_view feature = R"FEATURE(
Feature: After-ALL survives a failing Scenario
  Scenario: This one fails
    Given a valid setup
    When it fails

  Scenario: This one passes
    Given a valid setup
    Then it passes
)FEATURE";

    std::vector<std::string> collected;
    const GherkinFailureCallback collectInsteadOfExiting = [&collected](std::string_view message) {
        collected.emplace_back(message);
    };

    const FeatureResult result =
        RunFeature(feature, registry, "suite-after-all-on-failure.feature", collectInsteadOfExiting);

    // The Feature-level outcome genuinely reflects the failure...
    EXPECT_FALSE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);
    EXPECT_FALSE(result.scenarioResults[0].allPassed);
    EXPECT_TRUE(result.scenarioResults[1].allPassed);
    ASSERT_EQ(collected.size(), 1u);

    // ...but because onFailure here returns normally instead of exiting,
    // RunFeature keeps going all the way through both its prologue AND its
    // epilogue: Before-ALL already ran (as always), and After-ALL still
    // fires exactly once despite the first Scenario having failed - the
    // documented guarantee that After-ALL hooks are for suite-wide cleanup
    // and are only SAFE to rely on with a non-exiting onFailure.
    EXPECT_EQ(beforeAllCount, 1);
    EXPECT_EQ(afterAllCount, 1);
}

TEST(GherkinSuiteHooks, BeforeAllCompletesBeforeAndAfterAllStartsAfterEveryScenarioUnderParallelDispatch) {
    StepRegistry registry;

    std::atomic<bool> beforeAllCompleted{false};
    std::atomic<int> scenariosFinishedCount{0};
    std::atomic<int> beforeAllCount{0};
    std::atomic<int> afterAllCount{0};
    std::atomic<bool> anyScenarioObservedBeforeAllNotYetDone{false};
    std::atomic<bool> afterAllObservedScenariosStillPending{false};
    // Retry attempts for Scenario C run sequentially, privately within that
    // one Scenario's own std::async task (see impl::RunScenarioWithRetries's
    // doc comment in bdd.hpp) - a plain (non-atomic) counter captured by
    // reference is safe here, matching GherkinRetry::
    // ParallelRetrySucceedsCorrectlyUnderConcurrentDispatch's own counterA/
    // counterB precedent above.
    int retryAttempts = 0;

    registry.AddBeforeAllHook([&]() {
        // Before-ALL itself runs serially/synchronously before the parallel
        // dispatch loop even begins (see RunFeature's prologue in bdd.hpp),
        // so there is nothing to race against here - this hook establishes
        // suite-wide state before any Scenario task could possibly read it.
        beforeAllCount.fetch_add(1, std::memory_order_relaxed);
        beforeAllCompleted.store(true, std::memory_order_release);
    });
    registry.AddAfterAllHook([&]() {
        // By the time After-ALL runs, every dispatched Scenario has already
        // been joined via futures[i].get() inside RunFeature - A and B
        // (2 non-retrying Scenarios) must both have reached their own Then
        // step exactly once, AND Scenario C's entire @retry:2 loop must have
        // fully run its course (both attempts, ending in the passing one),
        // never just its first attempt.
        if (scenariosFinishedCount.load(std::memory_order_acquire) != 2 || retryAttempts != 2) {
            afterAllObservedScenariosStillPending.store(true, std::memory_order_relaxed);
        }
        afterAllCount.fetch_add(1, std::memory_order_relaxed);
    });

    registry.RegisterGiven("a valid setup", [&](TestContext&) -> bool {
        if (!beforeAllCompleted.load(std::memory_order_acquire)) {
            anyScenarioObservedBeforeAllNotYetDone.store(true, std::memory_order_relaxed);
        }
        return true;
    });
    // Retried by Scenario C's own std::async task on every @retry:2 attempt:
    // fails attempt 1 (retryAttempts becomes 1), passes attempt 2
    // (retryAttempts becomes 2) - see impl::RunScenarioWithRetries.
    registry.RegisterWhen("it eventually succeeds after one retry", [&retryAttempts](TestContext&) -> bool {
        return ++retryAttempts >= 2;
    });
    registry.RegisterWhen("it waits {int} ms then completes", [](TestContext&, int ms) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return true;
    });
    // Only A and B reach this step - it feeds scenariosFinishedCount, used
    // above to confirm After-ALL never runs while either is still pending.
    registry.RegisterThen("it completes", [&](TestContext&) -> bool {
        scenariosFinishedCount.fetch_add(1, std::memory_order_release);
        return true;
    });
    // Scenario C's own Then step, deliberately NOT touching
    // scenariosFinishedCount: BabyBehave's Gherkin runtime forces
    // collect-failures mode (see the file-header comment above), so a Then
    // step still runs even after an earlier failing When WITHIN the same
    // attempt - a shared "it completes" step would therefore fire once per
    // @retry attempt (2 times), not once per Scenario, making it useless as
    // a "Scenario C has fully finished" signal. retryAttempts itself (final
    // value == 2, checked above) is the real, unambiguous "all attempts
    // done" signal for C.
    registry.RegisterThen("the retried scenario completes", [](TestContext&) -> bool { return true; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Suite hooks under parallel dispatch
  Scenario: A - fast
    Given a valid setup
    When it waits 0 ms then completes
    Then it completes

  Scenario: B - slower
    Given a valid setup
    When it waits 60 ms then completes
    Then it completes

  @retry:2
  Scenario: C - retried once
    Given a valid setup
    When it eventually succeeds after one retry
    Then the retried scenario completes
)FEATURE";

    const GherkinFailureCallback failIfInvoked = [](std::string_view message) {
        ADD_FAILURE() << "onFailure must not be invoked for an all-passing parallel Feature: " << message;
    };

    const FeatureResult result = RunFeature(feature, registry, "suite-hooks-parallel.feature", failIfInvoked,
                                              /*enableParallelScenarios=*/true);

    ASSERT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 3u);
    EXPECT_EQ(beforeAllCount.load(), 1);
    EXPECT_EQ(afterAllCount.load(), 1);
    EXPECT_EQ(scenariosFinishedCount.load(), 2);
    EXPECT_EQ(retryAttempts, 2);
    EXPECT_FALSE(anyScenarioObservedBeforeAllNotYetDone.load())
        << "a Scenario step ran before Before-ALL had finished";
    EXPECT_FALSE(afterAllObservedScenariosStillPending.load())
        << "After-ALL ran while a Scenario (or one of its @retry attempts) was still pending";
}

TEST(GherkinSuiteHooks, CoexistsWithPerScenarioAndExpressionHooksAtTheirOwnUnaffectedScope) {
    StepRegistry registry;
    registry.RegisterGiven("a step", [](TestContext&) -> bool { return true; });

    int beforeAllCount = 0;
    int afterAllCount = 0;
    int perScenarioBeforeCount = 0;
    int perScenarioAfterCount = 0;
    int exprBeforeCount = 0;
    int exprAfterCount = 0;

    registry.AddBeforeAllHook([&beforeAllCount]() { ++beforeAllCount; });
    registry.AddAfterAllHook([&afterAllCount]() { ++afterAllCount; });
    registry.AddBeforeHook({}, [&perScenarioBeforeCount](TestContext&) { ++perScenarioBeforeCount; });
    registry.AddAfterHook({}, [&perScenarioAfterCount](TestContext&) { ++perScenarioAfterCount; });
    registry.AddBeforeHookExpr("@special", [&exprBeforeCount](TestContext&) { ++exprBeforeCount; });
    registry.AddAfterHookExpr("@special", [&exprAfterCount](TestContext&) { ++exprAfterCount; });

    constexpr std::string_view feature = R"FEATURE(
Feature: Suite hooks coexist with per-Scenario hooks
  Scenario: Untagged
    Given a step

  @special
  Scenario: Tagged
    Given a step
)FEATURE";

    const FeatureResult result = RunFeature(feature, registry, "suite-hooks-coexist.feature");

    EXPECT_TRUE(result.allPassed);
    ASSERT_EQ(result.scenarioResults.size(), 2u);

    // Suite-level: exactly once each, regardless of the 2 Scenarios present.
    EXPECT_EQ(beforeAllCount, 1);
    EXPECT_EQ(afterAllCount, 1);
    // Per-Scenario, untagged: once per Scenario (2 total).
    EXPECT_EQ(perScenarioBeforeCount, 2);
    EXPECT_EQ(perScenarioAfterCount, 2);
    // Per-Scenario, tag-expression-filtered: only the @special Scenario (1 total).
    EXPECT_EQ(exprBeforeCount, 1);
    EXPECT_EQ(exprAfterCount, 1);
}
