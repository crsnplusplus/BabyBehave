// test_Gherkin_Parser.cpp
//
// Unit-level tests of BabyBehave::BDD::Gherkin's ".feature" text parser and
// cucumber-expression-lite pattern compiler, IN ISOLATION from the
// StepRegistry/RunFeature execution machinery covered by
// test_Gherkin_Integration.cpp.
//
// These tests call straight into BabyBehave::BDD::Gherkin::impl (the
// header's own comment calls this "internal parsing/matching machinery,
// not part of the public surface" - see bdd.hpp) rather than going through
// the public RunFeature() entry point. That is deliberate, not a shortcut:
// RunFeature() is fail-hard for a malformed feature (it prints a
// diagnostic and calls std::exit(EXIT_FAILURE) - see bdd.hpp's RunFeature()
// and impl::ReportScenarioFailureAndExit()), so exercising every rejected
// construct through it would require a death test per case. Calling
// impl::ParseFeatureText() directly instead lets every parse-only behavior
// (accepted constructs, comments/tags, and every one of the four rejected
// constructs) be asserted on directly as an ordinary ParseOutcome value,
// with no process exit involved. test_Gherkin_Integration.cpp separately
// confirms RunFeature() itself relays a parse failure the same way
// end-to-end (see GherkinIntegrationDeathTest.MalformedFeatureFailsHardThroughRunFeature).
#include <BabyBehave/bdd.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <regex>
#include <string>
#include <string_view>

namespace GherkinImpl = BabyBehave::BDD::Gherkin::impl;

namespace {

// GherkinImpl::ParseOutcome::errors is a vector (parsing now accumulates
// every distinct structural error instead of stopping at the first one -
// see ParseOutcome's doc comment in bdd.hpp). This helper is only for
// gtest diagnostic streaming (`<< JoinErrors(outcome.errors)`); test
// assertions that need to inspect an individual message should index
// outcome.errors directly (typically after asserting its exact size, to
// also positively confirm no unintended cascade happened).
std::string JoinErrors(const std::vector<std::string>& errors) {
    std::string joined;
    for (const std::string& error : errors) {
        if (!joined.empty()) joined += " | ";
        joined += error;
    }
    return joined;
}

}  // namespace

// ---------------------------------------------------------------------
// Accepted constructs: Feature:/Background:/Scenario:/steps/tags/comments
// ---------------------------------------------------------------------

TEST(GherkinParser, ParsesMinimalFeatureScenarioAndStep) {
    constexpr std::string_view text = R"FEATURE(
Feature: Shopping basket
  Scenario: Adding an item
    Given an empty basket
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    EXPECT_EQ(outcome.feature.name, "Shopping basket");
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    const auto& scenario = outcome.feature.scenarios[0];
    EXPECT_EQ(scenario.name, "Adding an item");
    ASSERT_EQ(scenario.steps.size(), 1u);
    EXPECT_EQ(scenario.steps[0].keyword, GherkinImpl::StepKeyword::Given);
    EXPECT_EQ(scenario.steps[0].text, "an empty basket");
}

TEST(GherkinParser, CommentsAndBlankLinesAreIgnored) {
    constexpr std::string_view text = R"FEATURE(
# top-level comment, before Feature:
Feature: Shopping basket

  # a comment describing the scenario

  Scenario: Adding an item
    # a comment right above a step
    Given an empty basket

    # a comment between steps
    When I add 1 apple
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    // Only the two real step lines should have been picked up; every
    // comment/blank line above and between them must be silently skipped,
    // not misread as a step or as free text that breaks anything.
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "an empty basket");
    EXPECT_EQ(outcome.feature.scenarios[0].steps[1].text, "I add 1 apple");
}

TEST(GherkinParser, TagsAreRecordedPerScopeWithoutInheritanceAtParseTime) {
    // Tag INHERITANCE (Feature tags unioned into every Scenario) is
    // impl::UnionTags()'s job, applied by RunFeature() - see
    // test_Gherkin_Integration.cpp's TagUnionMakesFeatureLevelTagsInheritedByEveryScenario.
    // The parser itself must keep the two tag sets separate and unmodified,
    // which is what this test pins down.
    constexpr std::string_view text = R"FEATURE(
@featuretag
Feature: Tagged feature
  @scenariotag
  Scenario: Tagged scenario
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.tags.size(), 1u);
    EXPECT_EQ(outcome.feature.tags[0], "featuretag");
    ASSERT_EQ(outcome.feature.scenarios[0].tags.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].tags[0], "scenariotag");
    // Neither list leaked into the other.
    EXPECT_EQ(outcome.feature.tags[0], "featuretag");
}

TEST(GherkinParser, MultipleTagsAcrossMultipleLinesAccumulate) {
    constexpr std::string_view text = R"FEATURE(
Feature: Multi-tag feature
  @slow @integration
  @needs-db
  Scenario: Heavily tagged
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    const auto& tags = outcome.feature.scenarios[0].tags;
    ASSERT_EQ(tags.size(), 3u);
    EXPECT_EQ(tags[0], "slow");
    EXPECT_EQ(tags[1], "integration");
    EXPECT_EQ(tags[2], "needs-db");
}

TEST(GherkinParser, BackgroundStepsAreSeparateFromScenarioSteps) {
    constexpr std::string_view text = R"FEATURE(
Feature: Coffee machine
  Background:
    Given a freshly booted coffee machine
    And a full water tank
  Scenario: Brewing an espresso
    When I brew an espresso
    Then a cup is served
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.background.size(), 2u);
    EXPECT_EQ(outcome.feature.background[0].text, "a freshly booted coffee machine");
    EXPECT_EQ(outcome.feature.background[1].text, "a full water tank");

    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "I brew an espresso");
    EXPECT_EQ(outcome.feature.scenarios[0].steps[1].text, "a cup is served");
}

TEST(GherkinParser, MultipleScenariosEachGetTheirOwnSteps) {
    constexpr std::string_view text = R"FEATURE(
Feature: Coffee machine
  Scenario: First
    Given step one
  Scenario: Second
    Given step two
    When step three
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "First");
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[1].name, "Second");
    ASSERT_EQ(outcome.feature.scenarios[1].steps.size(), 2u);
}

TEST(GherkinParser, ExampleKeywordIsAcceptedAsScenarioSynonym) {
    constexpr std::string_view text = R"FEATURE(
Feature: Synonyms
  Example: Using Example instead of Scenario
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "Using Example instead of Scenario");
}

TEST(GherkinParser, AllFiveStepKeywordsAreClassifiedCorrectly) {
    constexpr std::string_view text = R"FEATURE(
Feature: Keywords
  Scenario: One of each
    Given a given step
    When a when step
    Then a then step
    And an and step
    But a but step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    const auto& steps = outcome.feature.scenarios[0].steps;
    ASSERT_EQ(steps.size(), 5u);
    EXPECT_EQ(steps[0].keyword, GherkinImpl::StepKeyword::Given);
    EXPECT_EQ(steps[1].keyword, GherkinImpl::StepKeyword::When);
    EXPECT_EQ(steps[2].keyword, GherkinImpl::StepKeyword::Then);
    EXPECT_EQ(steps[3].keyword, GherkinImpl::StepKeyword::And);
    EXPECT_EQ(steps[4].keyword, GherkinImpl::StepKeyword::But);
}

// ---------------------------------------------------------------------
// Line/column tracking: this is one of the two "known bug fixes" called
// out for this work (StepResult::location must reflect the real .feature
// position, not the interpreter's internal dispatch site). Asserting on
// literal, hand-counted line/column numbers below - rather than just
// "non-empty" - is what would actually catch a regression to "always the
// same value" or "always empty".
// ---------------------------------------------------------------------

TEST(GherkinParser, StepLineAndColumnTrackingReflectsRealPosition) {
    // Line numbering, counted by hand against SplitLines()'s 1-based
    // scheme (blank line 1 comes from the '\n' immediately after the
    // opening raw-string delimiter):
    //   1: ""
    //   2: "Feature: Location tracking"
    //   3: "  Background:"
    //   4: "    Given a background precondition"
    //   5: "  Scenario: Single scenario"
    //   6: "      When I do something"      (6 leading spaces -> column 7)
    //   7: "    Then it should work"        (4 leading spaces -> column 5)
    constexpr std::string_view text = R"FEATURE(
Feature: Location tracking
  Background:
    Given a background precondition
  Scenario: Single scenario
      When I do something
    Then it should work
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);

    ASSERT_EQ(outcome.feature.background.size(), 1u);
    EXPECT_EQ(outcome.feature.background[0].line, 4u);
    EXPECT_EQ(outcome.feature.background[0].column, 5u);

    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].line, 5u);

    const auto& steps = outcome.feature.scenarios[0].steps;
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].line, 6u);
    EXPECT_EQ(steps[0].column, 7u);
    EXPECT_EQ(steps[1].line, 7u);
    EXPECT_EQ(steps[1].column, 5u);

    // The two steps' locations must actually differ from each other - a
    // parser that hard-codes/collapses line or column tracking would make
    // this fail even though each individual EXPECT_EQ above might
    // (coincidentally) still pass in a simpler broken feature file.
    EXPECT_NE(steps[0].line, steps[1].line);
    EXPECT_NE(steps[0].column, steps[1].column);
}

TEST(GherkinParser, CRLFLineEndingsAreTolerated) {
    const std::string text = "\r\nFeature: CRLF\r\n  Scenario: Works\r\n    Given a step\r\n";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    EXPECT_EQ(outcome.feature.name, "CRLF");
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    // The trailing '\r' must not have leaked into the step text.
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a step");
}

TEST(GherkinParser, FreeTextDescriptionLinesAreIgnoredNotRejected) {
    constexpr std::string_view text = R"FEATURE(
Feature: Basket
  As a shopper
  I want to add items to my basket
  So that I can buy them later

  Scenario: Adding an item
    Given an empty basket
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    EXPECT_EQ(outcome.feature.name, "Basket");
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
}

// ---------------------------------------------------------------------
// Rejected constructs: each must be a clean parse error (ok == false,
// non-empty diagnostic), never a crash and never silently ignored/
// misparsed as something else.
// ---------------------------------------------------------------------

TEST(GherkinParser, RejectsMissingFeatureSection) {
    constexpr std::string_view text = R"FEATURE(
Scenario: Orphaned scenario
  Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("no 'Feature:' found"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, RejectsMultipleFeatureSections) {
    constexpr std::string_view text = R"FEATURE(
Feature: First
  Scenario: A
    Given a step
Feature: Second
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("multiple 'Feature:' sections"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, RejectsStepOutsideBackgroundOrScenario) {
    constexpr std::string_view text = R"FEATURE(
Feature: Misplaced step
  Given a step with nowhere to attach
  Scenario: A
    When something happens
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step found outside of a Background:/Scenario:"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, RejectsRuleConstruct) {
    constexpr std::string_view text = R"FEATURE(
Feature: With a Rule
  Rule: Some business rule
  Scenario: A
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("'Rule:' is not supported"), std::string::npos) << outcome.errors.front();
}

// ---------------------------------------------------------------------
// Scenario Outline/Examples: each Scenario Outline:/Scenario Template: is
// expanded, entirely at parse time, into one ordinary ParsedScenario per
// Examples:/Scenarios: data row - see impl::ExpandScenarioOutlines. By the
// time ParseFeatureText returns, an expanded row is indistinguishable from
// a hand-written Scenario (its own .examples is nullopt), which is why
// these tests assert directly on outcome.feature.scenarios rather than on
// any outline-specific accessor.
// ---------------------------------------------------------------------

TEST(GherkinParser, ScenarioOutlineExpandsToOneScenarioPerExamplesRow) {
    // Line numbering (1-based, blank line 1 from the raw-string opener):
    //   1: ""
    //   2: "Feature: Basket"
    //   3: "  Scenario Outline: Adding <count> <item>"
    //   4: "    Given an empty basket"
    //   5: "    When I add <count> <item>"
    //   6: "    Then the basket has <count> <item>"
    //   7: ""
    //   8: "  Examples:"
    //   9: "    | count | item   |"    (header)
    //  10: "    | 1     | apple  |"    (row 1)
    //  11: "    | 2     | orange |"    (row 2)
    constexpr std::string_view text = R"FEATURE(
Feature: Basket
  Scenario Outline: Adding <count> <item>
    Given an empty basket
    When I add <count> <item>
    Then the basket has <count> <item>

  Examples:
    | count | item   |
    | 1     | apple  |
    | 2     | orange |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);

    const auto& row1 = outcome.feature.scenarios[0];
    EXPECT_EQ(row1.name, "Adding <count> <item> (Examples row 1)");
    EXPECT_FALSE(row1.examples.has_value()) << "expanded rows must not carry their own Examples table";
    EXPECT_EQ(row1.line, 10u) << "expanded scenario's line is the ROW's own source line";
    ASSERT_EQ(row1.steps.size(), 3u);
    EXPECT_EQ(row1.steps[0].text, "an empty basket");
    EXPECT_EQ(row1.steps[1].text, "I add 1 apple");
    EXPECT_EQ(row1.steps[2].text, "the basket has 1 apple");
    // Every expanded step's line/column stay anchored to the TEMPLATE
    // step's own position, not the row's.
    EXPECT_EQ(row1.steps[0].line, 4u);
    EXPECT_EQ(row1.steps[0].column, 5u);
    EXPECT_EQ(row1.steps[1].line, 5u);
    EXPECT_EQ(row1.steps[2].line, 6u);

    const auto& row2 = outcome.feature.scenarios[1];
    EXPECT_EQ(row2.name, "Adding <count> <item> (Examples row 2)");
    EXPECT_EQ(row2.line, 11u);
    ASSERT_EQ(row2.steps.size(), 3u);
    EXPECT_EQ(row2.steps[1].text, "I add 2 orange");
    EXPECT_EQ(row2.steps[2].text, "the basket has 2 orange");
    // Template step line/column are shared across every expanded row.
    EXPECT_EQ(row2.steps[1].line, row1.steps[1].line);
    EXPECT_EQ(row2.steps[1].column, row1.steps[1].column);
}

// Regression coverage: impl::SubstitutePlaceholders's "no closing '>'"
// branch - a '<' with no matching '>' anywhere after it in the step text is
// left as literal text (the whole remainder, verbatim), the same
// "when in doubt, don't add a new failure mode" treatment an unmatched
// column name (<missing> with no matching header column - see
// test_Gherkin_Integration.cpp's UnmatchedStepFailsHardWithClearMessage)
// already gets.
TEST(GherkinParser, UnterminatedAngleBracketInOutlineStepIsLeftAsLiteralText) {
    constexpr std::string_view text = R"FEATURE(
Feature: Unterminated placeholder
  Scenario Outline: Has a stray '<' with no closing '>'
    Given a value of <x and more

  Examples:
    | x |
    | 1 |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a value of <x and more");
}

TEST(GherkinParser, ScenarioTemplateAndScenariosSynonymsAcceptedIdenticallyToOutlineAndExamples) {
    constexpr std::string_view text = R"FEATURE(
Feature: Synonyms
  Scenario Template: Templated <thing>
    Given a <thing>

  Scenarios:
    | thing  |
    | widget |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "Templated <thing> (Examples row 1)");
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a widget");
}

TEST(GherkinParser, ExamplesRowCellCountMismatchIsAParseErrorAtTheRowsLine) {
    // Row "| widget | extra |" (3 cells incl. trailing split) is on line 8:
    //   1: "" 2: "Feature: Mismatch" 3: "  Scenario Outline: Bad row"
    //   4: "    Given a <thing>" 5: "" 6: "  Examples:" 7: "    | thing |"
    //   8: "    | widget | extra |"
    constexpr std::string_view text = R"FEATURE(
Feature: Mismatch
  Scenario Outline: Bad row
    Given a <thing>

  Examples:
    | thing |
    | widget | extra |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    // RecordParseError's format is "<lineNo>: parse error: <message>" (the
    // "<file>:" label prefix is only prepended later, by RunFeature() -
    // see ParseErrorsIncludeTheRealLineNumber below for that split
    // responsibility spelled out explicitly), so the line number shows up
    // as a bare "8:" prefix rather than the literal text "line 8:".
    EXPECT_NE(outcome.errors.front().find("8: parse error:"), std::string::npos) << outcome.errors.front();
    EXPECT_NE(outcome.errors.front().find("cell(s)"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, ScenarioOutlineWithNoExamplesAtAllIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Missing examples
  Scenario Outline: No examples here
    Given a <thing>
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("has no 'Examples:'/'Scenarios:' table"), std::string::npos) << outcome.errors.front();
}

// Regression coverage: impl::FinalizeCurrentScenarioExamples's "no
// Examples: table" check runs from THREE call sites - end-of-file (already
// covered by ScenarioOutlineWithNoExamplesAtAllIsAParseError above),
// impl::HandleScenarioHeaderLine (a NEW 'Scenario:'/'Scenario Outline:'
// line flushes whatever Outline came before it), and the 'Background:'
// line handling in impl::ProcessFeatureLine (same flush, different
// trigger) - these two tests hit those other call sites specifically.
TEST(GherkinParser, ScenarioOutlineWithNoExamplesFollowedByAnotherScenarioIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Missing examples before another Scenario
  Scenario Outline: No examples here
    Given a <thing>

  Scenario: The next one
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("has no 'Examples:'/'Scenarios:' table"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, ScenarioOutlineWithNoExamplesFollowedByBackgroundIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Missing examples before a Background
  Scenario Outline: No examples here
    Given a <thing>

  Background:
    Given a setup step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("has no 'Examples:'/'Scenarios:' table"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, ExamplesWithHeaderOnlyAndZeroDataRowsIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Empty examples
  Scenario Outline: No rows
    Given a <thing>

  Examples:
    | thing |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("at least one data row"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, ExamplesAfterAPlainScenarioNotAnOutlineIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Not an outline
  Scenario: A
    Given a step

  Examples:
    | thing  |
    | widget |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("without a preceding 'Scenario Outline:'/'Scenario Template:'"), std::string::npos)
        << outcome.errors.front();
}

TEST(GherkinParser, MultipleScenarioOutlinesEachExpandIndependentlyPreservingDeclarationOrder) {
    constexpr std::string_view text = R"FEATURE(
Feature: Multiple outlines
  Scenario Outline: First <n>
    Given step <n>

  Examples:
    | n |
    | 1 |
    | 2 |

  Scenario: Plain in between
    Given a plain step

  Scenario Outline: Second <n>
    Given other step <n>

  Examples:
    | n |
    | 3 |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 4u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "First <n> (Examples row 1)");
    EXPECT_EQ(outcome.feature.scenarios[1].name, "First <n> (Examples row 2)");
    EXPECT_EQ(outcome.feature.scenarios[2].name, "Plain in between");
    EXPECT_EQ(outcome.feature.scenarios[3].name, "Second <n> (Examples row 1)");
}

TEST(GherkinParser, FeatureEndingWhileStillInsideAnExamplesTableStillParsesCorrectly) {
    // Regression test: FinalizeCurrentScenarioExamples() must run at the
    // end-of-file path too (ParseFeatureText's tail), not just the
    // Background:/Scenario: dispatch branches - a file that ends right
    // after the last Examples row, with no blank line or following section,
    // must still finalize the table and expand normally instead of silently
    // dropping the in-progress Outline.
    constexpr std::string_view text = R"FEATURE(
Feature: EOF inside examples
  Scenario Outline: Ends abruptly <thing>
    Given a <thing>

  Examples:
    | thing  |
    | widget |)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "Ends abruptly <thing> (Examples row 1)");
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a widget");
}

TEST(GherkinParser, CRLFLineEndingsInsideAnExamplesRowAreTolerated) {
    const std::string text =
        "\r\nFeature: CRLF Examples\r\n"
        "  Scenario Outline: CRLF <thing>\r\n"
        "    Given a <thing>\r\n"
        "\r\n"
        "  Examples:\r\n"
        "    | thing  |\r\n"
        "    | widget |\r\n";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].name, "CRLF <thing> (Examples row 1)");
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    // The trailing '\r' inside each pipe cell must not have leaked into
    // either the substituted step text or the cell used for substitution.
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a widget");
}

TEST(GherkinParser, MultipleExamplesBlocksOnOneOutlineMergeAllRowsInDeclarationOrder) {
    // Regression test for a bug where a second (or later) Examples:/
    // Scenarios: block on the same Scenario Outline silently overwrote
    // (rather than merged with) the first - only the LAST block's rows ever
    // produced scenarios. Real Cucumber supports multiple named Examples:
    // blocks per outline, with every row from every block used.
    constexpr std::string_view text = R"FEATURE(
Feature: Multiple examples blocks
  Scenario Outline: Adding <n>
    Given step <n>

  Examples: First batch
    | n |
    | 1 |
    | 2 |

  Examples: Second batch
    | n |
    | 3 |
    | 4 |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 4u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "step 1");
    EXPECT_EQ(outcome.feature.scenarios[1].steps[0].text, "step 2");
    EXPECT_EQ(outcome.feature.scenarios[2].steps[0].text, "step 3");
    EXPECT_EQ(outcome.feature.scenarios[3].steps[0].text, "step 4");
}

TEST(GherkinParser, SecondExamplesBlockHeaderCellCountMismatchIsAParseError) {
    // The second block's own header row must still be validated against the
    // column count already established by the first block.
    constexpr std::string_view text = R"FEATURE(
Feature: Mismatched second block
  Scenario Outline: Adding <n>
    Given step <n>

  Examples:
    | n |
    | 1 |

  Examples:
    | n | extra |
    | 2 | 3     |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("cell(s)"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, StepKeywordAfterExamplesTableOnSameOutlineIsAParseError) {
    // Regression test for a bug where a step line reached after this
    // Outline's own (already-closed) Examples: table was silently
    // reattached to the Outline scenario, and any trailing '|' lines after
    // it were misread as a fresh Examples row/Data Table - producing 3
    // scenarios (with a bogus step and misread pipe rows) from what should
    // be a single, clearly rejected malformed feature.
    constexpr std::string_view text = R"FEATURE(
Feature: Step after examples
  Scenario Outline: X
    Given a <n>
  Examples:
    | n |
    | 1 |
    Given something weird after examples
      | a |
      | zzz |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step keyword found after"), std::string::npos) << outcome.errors.front();
}

// ---------------------------------------------------------------------
// Data Tables (impl::HandleDataTableLine / ParsedStep::rawArgument): a
// pipe-row block immediately following a step is attached to that step -
// as opposed to the OLD (pre-Feature-4) behavior of hard-rejecting any '|'
// line outside an Examples:/Scenarios: table. Doc Strings (Feature 5) are
// covered separately below (impl::HandleDocStringLine), after
// ParseErrorsIncludeTheRealLineNumber.
// ---------------------------------------------------------------------

TEST(GherkinParser, DataTableIsAttachedToImmediatelyPrecedingStep) {
    constexpr std::string_view text = R"FEATURE(
Feature: Data tables
  Scenario: A
    Given the following items
      | name  | qty |
      | apple | 3   |
      | pear  | 5   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    const GherkinImpl::ParsedStep& step = outcome.feature.scenarios[0].steps[0];
    ASSERT_TRUE(std::holds_alternative<BabyBehave::BDD::Gherkin::DataTable>(step.rawArgument));
    const auto& table = std::get<BabyBehave::BDD::Gherkin::DataTable>(step.rawArgument);
    ASSERT_EQ(table.rows.size(), 3u);
    EXPECT_EQ(table.Header(), (std::vector<std::string>{ "name", "qty" }));
    EXPECT_EQ(table.RowCount(), 2u);
    EXPECT_EQ(table.Row(0), (std::vector<std::string>{ "apple", "3" }));
    EXPECT_EQ(table.Row(1), (std::vector<std::string>{ "pear", "5" }));
}

TEST(GherkinParser, DataTableAttachedToABackgroundStep) {
    constexpr std::string_view text = R"FEATURE(
Feature: Data tables in Background
  Background:
    Given the following items
      | name  |
      | apple |
  Scenario: A
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.background.size(), 1u);
    const GherkinImpl::ParsedStep& backgroundStep = outcome.feature.background[0];
    ASSERT_TRUE(std::holds_alternative<BabyBehave::BDD::Gherkin::DataTable>(backgroundStep.rawArgument));
    // The plain Scenario step must NOT have picked up any raw argument.
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(outcome.feature.scenarios[0].steps[0].rawArgument));
}

TEST(GherkinParser, DataTableWithNoPrecedingStepAtStartOfScenarioIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: No preceding step
  Scenario: A
    | name  | qty |
    | apple | 3   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("data table with no preceding step"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, DataTableWithNoPrecedingStepRightAfterFeatureIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: No preceding step at all
  | name  | qty |
  | apple | 3   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("data table with no preceding step"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, SecondDataTableBlockOnTheSameStepIsAParseError) {
    // A blank line ends row-accumulation for the FIRST table (see
    // FeatureParseState::inDataTable); the next '|' line is then
    // re-evaluated against the step it would attach to, which already
    // carries a DataTable from the first block.
    constexpr std::string_view text = R"FEATURE(
Feature: Two tables on one step
  Scenario: A
    Given the following items
      | name  | qty |
      | apple | 3   |

      | name  | qty |
      | pear  | 5   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step already has an argument"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, ExamplesTableAndDataTableCoexistWithoutInterference) {
    // A Data Table attached to a Scenario Outline's template step and an
    // Examples:/Scenarios: table in the very same Outline must each go
    // through their own dedicated code path (HandleDataTableLine vs the
    // inExamplesTable '|' branch) with no cross-talk, and the Data Table
    // must be replicated (unmodified, no <name> substitution) onto every
    // expanded row's copy of that step (see ExpandScenarioOutlines).
    constexpr std::string_view text = R"FEATURE(
Feature: Outline with both constructs
  Scenario Outline: Adding <item>
    Given the following items
      | name  |
      | fixed |
    When I add <item>

  Examples:
    | item   |
    | apple  |
    | orange |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    for (const auto& row : outcome.feature.scenarios) {
        ASSERT_EQ(row.steps.size(), 2u);
        ASSERT_TRUE(std::holds_alternative<BabyBehave::BDD::Gherkin::DataTable>(row.steps[0].rawArgument));
        const auto& table = std::get<BabyBehave::BDD::Gherkin::DataTable>(row.steps[0].rawArgument);
        EXPECT_EQ(table.Header(), (std::vector<std::string>{ "name" }));
        EXPECT_EQ(table.RowCount(), 1u);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(row.steps[1].rawArgument));
    }
    EXPECT_EQ(outcome.feature.scenarios[0].steps[1].text, "I add apple");
    EXPECT_EQ(outcome.feature.scenarios[1].steps[1].text, "I add orange");
}

TEST(GherkinParser, DataTableRowWithTooManyCellsIsAParseError) {
    // Mirrors the Examples: table's header/row width check: a Data Table's
    // FIRST row establishes the expected cell count, and every later row
    // must match it exactly - unlike a ragged table silently producing
    // rows of different sizes (which would only surface much later as a
    // confusing std::out_of_range from DataTable::Get inside arbitrary
    // user step code).
    constexpr std::string_view text = R"FEATURE(
Feature: Ragged data table (too many cells)
  Scenario: A
    Given the following items
      | name  | qty |
      | apple | 3   | extra |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("data table row has 3 cell(s), expected 2 (from header)"), std::string::npos)
        << outcome.errors.front();
}

TEST(GherkinParser, DataTableRowWithTooFewCellsIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Ragged data table (too few cells)
  Scenario: A
    Given the following items
      | name  | qty |
      | pear  |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("data table row has 1 cell(s), expected 2 (from header)"), std::string::npos)
        << outcome.errors.front();
}

TEST(GherkinParser, DataTableWithConsistentCellCountsAcrossManyRowsParsesFine) {
    // No false-positive regression: a well-formed, same-width multi-row
    // Data Table (beyond the 2-data-row case already covered by
    // DataTableIsAttachedToImmediatelyPrecedingStep) must still parse
    // cleanly once the width check is in place.
    constexpr std::string_view text = R"FEATURE(
Feature: Consistent data table
  Scenario: A
    Given the following items
      | name   | qty |
      | apple  | 3   |
      | pear   | 5   |
      | banana | 7   |
      | grape  | 9   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    const auto& table = std::get<BabyBehave::BDD::Gherkin::DataTable>(outcome.feature.scenarios[0].steps[0].rawArgument);
    ASSERT_EQ(table.RowCount(), 4u);
    EXPECT_EQ(table.Row(3), (std::vector<std::string>{ "grape", "9" }));
}

TEST(GherkinParser, DataTableCellsSupportBackslashEscapedPipeAndBackslash) {
    // Cucumber's standard cell-escaping (impl::ParsePipeRow/UnescapePipeCell):
    // '\|' is a literal pipe inside a cell, not a column delimiter; '\\' is
    // a literal backslash.
    constexpr std::string_view text = R"FEATURE(
Feature: Escaped cells
  Scenario: A
    Given the following items
      | name        | note            |
      | a\|b        | back\\slash     |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    const auto& table = std::get<BabyBehave::BDD::Gherkin::DataTable>(outcome.feature.scenarios[0].steps[0].rawArgument);
    ASSERT_EQ(table.RowCount(), 1u);
    EXPECT_EQ(table.Header(), (std::vector<std::string>{ "name", "note" }));
    EXPECT_EQ(table.Row(0), (std::vector<std::string>{ "a|b", "back\\slash" }));
}

TEST(GherkinParser, ExamplesTableCellsSupportBackslashEscapedPipe) {
    constexpr std::string_view text = R"FEATURE(
Feature: Escaped examples cell
  Scenario Outline: Adding <thing>
    Given <thing>

  Examples:
    | thing |
    | a\|b  |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a|b");
}

// ---------------------------------------------------------------------
// Gherkin::DataTable's own accessors (RowCount/Header/Row/Get), in
// isolation from the parser - a DataTable is just rows[0] == header,
// rows[1..] == data; nothing here depends on ParseFeatureText.
// ---------------------------------------------------------------------

TEST(GherkinDataTable, RowCountHeaderAndRowAccessorsReflectTheGrid) {
    const BabyBehave::BDD::Gherkin::DataTable table{
        .rows = { { "name", "qty" }, { "apple", "3" }, { "pear", "5" } }
    };

    EXPECT_EQ(table.RowCount(), 2u);
    EXPECT_EQ(table.Header(), (std::vector<std::string>{ "name", "qty" }));
    EXPECT_EQ(table.Row(0), (std::vector<std::string>{ "apple", "3" }));
    EXPECT_EQ(table.Row(1), (std::vector<std::string>{ "pear", "5" }));
}

TEST(GherkinDataTable, RowCountIsZeroForAnEmptyTable) {
    const BabyBehave::BDD::Gherkin::DataTable table;
    EXPECT_EQ(table.RowCount(), 0u);
}

TEST(GherkinDataTable, RowCountIsZeroForAHeaderOnlyTable) {
    const BabyBehave::BDD::Gherkin::DataTable table{ .rows = { { "name", "qty" } } };
    EXPECT_EQ(table.RowCount(), 0u);
}

TEST(GherkinDataTable, GetLooksUpCellByDataRowIndexAndColumnName) {
    const BabyBehave::BDD::Gherkin::DataTable table{
        .rows = { { "name", "qty" }, { "apple", "3" }, { "pear", "5" } }
    };

    EXPECT_EQ(table.Get(0, "name"), "apple");
    EXPECT_EQ(table.Get(0, "qty"), "3");
    EXPECT_EQ(table.Get(1, "name"), "pear");
    EXPECT_EQ(table.Get(1, "qty"), "5");
}

TEST(GherkinDataTable, GetWithUnknownColumnNameThrowsInvalidArgument) {
    const BabyBehave::BDD::Gherkin::DataTable table{ .rows = { { "name", "qty" }, { "apple", "3" } } };

    EXPECT_THROW({ [[maybe_unused]] auto unused = table.Get(0, "price"); }, std::invalid_argument);
}

TEST(GherkinDataTable, HeaderAndRowThrowOutOfRangeForInvalidIndices) {
    const BabyBehave::BDD::Gherkin::DataTable emptyTable;
    EXPECT_THROW({ [[maybe_unused]] const auto& unused = emptyTable.Header(); }, std::out_of_range);

    const BabyBehave::BDD::Gherkin::DataTable table{ .rows = { { "name" }, { "apple" } } };
    EXPECT_THROW({ [[maybe_unused]] const auto& unused = table.Row(5); }, std::out_of_range);
}

TEST(GherkinParser, ParseErrorsIncludeTheRealLineNumber) {
    // "Rule:" is on line 3 (1: "", 2: "Feature: ...", 3: "  Rule: ...").
    // Each impl::ParseOutcome::errors entry is "<lineNo>: parse error:
    // <message>" (RecordParseError) - it is RunFeature() that later
    // prepends the "<featureLabel>:" file label to get the full
    // "<file>:<line>: parse error: <message>" form (see RunFeature's parse
    // error loop in bdd.hpp and GherkinIntegrationDeathTest.
    // MalformedFeatureFailsHardThroughRunFeature /
    // CustomFailureCallbackForParseErrorReturnsInsteadOfExiting in
    // test_Gherkin_Integration.cpp, which check that end-to-end label).
    constexpr std::string_view text = R"FEATURE(
Feature: Line-numbered error
  Rule: Some rule
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("3: parse error:"), std::string::npos) << outcome.errors.front();
}

// ---------------------------------------------------------------------
// Doc Strings (impl::HandleDocStringLine / ParsedStep::rawArgument): a
// '"""'-delimited block immediately following a step is attached to that
// step as a single newline-joined std::string, with the OPENING '"""'
// line's own indentation stripped from every content line ("smart"
// indentation - see impl::StripDocStringIndent). Mirrors the Data Tables
// section above as closely as the two constructs' shapes allow, including
// the same "no preceding step"/"step already has an argument" checks and
// error wording (a step cannot have both a Data Table and a Doc String -
// whichever attaches first wins the slot).
// ---------------------------------------------------------------------

TEST(GherkinParser, DocStringIsAttachedWithOpeningDelimiterIndentationStripped) {
    // The opening """ below sits at column 7 (6 leading spaces). Every
    // content line has that much (or, for the blank line, as much as it
    // has - i.e. none) stripped: "Title: Multi-line" (exactly 6 spaces,
    // fully stripped), "  indented sub-point" (8 spaces in the source,
    // only 6 stripped - 2 remain, proving RELATIVE indentation survives),
    // a truly blank line (0 spaces, nothing to strip - preserved as an
    // empty line in the joined result), and "Back to base indentation."
    // (6 spaces, fully stripped again).
    constexpr std::string_view text = R"FEATURE(
Feature: Article publishing
  Scenario: Publish an article
    Given the article body is:
      """
      Title: Multi-line
        indented sub-point

      Back to base indentation.
      """
    When the article is published
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 2u);
    const GherkinImpl::ParsedStep& step = outcome.feature.scenarios[0].steps[0];
    ASSERT_TRUE(std::holds_alternative<std::string>(step.rawArgument));
    EXPECT_EQ(std::get<std::string>(step.rawArgument),
              "Title: Multi-line\n  indented sub-point\n\nBack to base indentation.");
    // The following step must NOT have picked up any raw argument.
    EXPECT_TRUE(std::holds_alternative<std::monostate>(outcome.feature.scenarios[0].steps[1].rawArgument));
}

TEST(GherkinParser, DocStringAttachedToABackgroundStep) {
    constexpr std::string_view text = R"FEATURE(
Feature: Doc strings in Background
  Background:
    Given the article body is:
      """
      shared body text
      """
  Scenario: A
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.background.size(), 1u);
    const GherkinImpl::ParsedStep& backgroundStep = outcome.feature.background[0];
    ASSERT_TRUE(std::holds_alternative<std::string>(backgroundStep.rawArgument));
    EXPECT_EQ(std::get<std::string>(backgroundStep.rawArgument), "shared body text");
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(outcome.feature.scenarios[0].steps[0].rawArgument));
}

TEST(GherkinParser, DocStringWithNoPrecedingStepIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: No preceding step
  Scenario: A
    """
    orphaned doc string
    """
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("doc string with no preceding step"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, SecondDocStringBlockOnTheSameStepIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Two doc strings on one step
  Scenario: A
    Given the article body is:
      """
      first block
      """
      """
      second block
      """
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step already has an argument"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, DocStringOnAStepThatAlreadyHasADataTableIsAParseError) {
    // Data-Table-then-DocString: the Data Table wins the step's single
    // rawArgument slot; the Doc String that immediately follows is a
    // second raw argument on the same step, same as a second Data Table
    // would be (see SecondDataTableBlockOnTheSameStepIsAParseError above).
    constexpr std::string_view text = R"FEATURE(
Feature: Data table then doc string
  Scenario: A
    Given the following items
      | name  |
      | apple |
      """
      not allowed - step already has a Data Table
      """
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step already has an argument"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, DataTableOnAStepThatAlreadyHasADocStringIsAParseError) {
    // DocString-then-DataTable: the reverse ordering of the case above -
    // still the same "step already has an argument" error, since a step's
    // rawArgument slot can hold at most one of either kind.
    constexpr std::string_view text = R"FEATURE(
Feature: Doc string then data table
  Scenario: A
    Given the article body is:
      """
      not allowed - step already has a Doc String
      """
      | name  |
      | apple |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("step already has an argument"), std::string::npos) << outcome.errors.front();
}

TEST(GherkinParser, DocStringContentContainingPipeAndHashCharactersIsNotMisparsed) {
    // Content lines starting with '|' or '#' must be treated as LITERAL
    // Doc String content, never as a Data Table row or a comment - the
    // parser must be in an unambiguous "everything until closing \"\"\" is
    // literal" mode while inside an open block.
    constexpr std::string_view text = R"FEATURE(
Feature: Doc string with table-/comment-like content
  Scenario: A
    Given the article body is:
      """
      | not | a | table | row |
      # not a comment either
      """
    When the article is published
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 2u);
    const GherkinImpl::ParsedStep& step = outcome.feature.scenarios[0].steps[0];
    ASSERT_TRUE(std::holds_alternative<std::string>(step.rawArgument));
    EXPECT_EQ(std::get<std::string>(step.rawArgument), "| not | a | table | row |\n# not a comment either");
}

TEST(GherkinParser, UnclosedDocStringIsAParseError) {
    constexpr std::string_view text = R"FEATURE(
Feature: Unclosed doc string
  Scenario: A
    Given the article body is:
      """
      never closed
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    // Exactly one accumulated error - positively confirms this malformed
    // construct's recovery path (point/intermediate/corruptive, see
    // ParseOutcome's doc comment) does not cascade into spurious extra
    // errors nor silently swallow the real one.
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("doc string is not closed"), std::string::npos) << outcome.errors.front();
}

// Unlike UnclosedDocStringIsAParseError above (only discovered at EOF), this
// one hits impl::ConsumeDocStringLine's own early corruptive detection: a
// NEW 'Scenario:' line arrives before the closing '"""' - the unclosed Doc
// String is reported right there (against its OPENING line), and parsing
// resumes normally with the new Scenario instead of silently swallowing it
// as more Doc String content.
TEST(GherkinParser, UnclosedDocStringIsDetectedEarlyAtANewScenarioBoundary) {
    constexpr std::string_view text = R"FEATURE(
Feature: Unclosed doc string hits a new Scenario before EOF
  Scenario: Opens but a new Scenario begins first
    Given a step
    """
    body never closes
  Scenario: A second scenario that parses normally
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("doc string is not closed"), std::string::npos) << outcome.errors.front();
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[1].steps.size(), 1u);
}

// Mirrors the case above but for a REJECTED doc string open attempt (no
// preceding step) instead of a genuinely-open one - see
// impl::ConsumeSkippedRejectedDocStringLine's own corruptive-resync
// handling: a NEW 'Scenario:' line reached before the matching closing '"""'
// stops the swallow and re-dispatches that boundary line normally, rather
// than letting the rejected attempt eat the whole next Scenario.
TEST(GherkinParser, RejectedDocStringOpenSwallowStopsAtANewScenarioBoundary) {
    constexpr std::string_view text = R"FEATURE(
Feature: Rejected doc string swallow stops at a block boundary
  Scenario: Doc string with no preceding step
    """
    stray content, never attached
  Scenario: A second scenario that parses normally
    Given a step
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("doc string with no preceding step"), std::string::npos)
        << outcome.errors.front();
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[1].steps.size(), 1u);
}

TEST(GherkinParser, ExpandScenarioOutlinesCopiesDocStringVerbatimToEveryExpandedRow) {
    // Same guarantee ExamplesTableAndDataTableCoexistWithoutInterference
    // above verifies for a Data Table: a Doc String attached to a Scenario
    // Outline's template step must be replicated unmodified (no <name>
    // substitution) onto every expanded row's copy of that step (see
    // impl::ExpandScenarioOutlines, which treats rawArgument generically
    // regardless of which alternative it holds).
    constexpr std::string_view text = R"FEATURE(
Feature: Outline with a doc string
  Scenario Outline: Publishing <item>
    Given the article body is:
      """
      shared template body
      """
    When I publish <item>

  Examples:
    | item   |
    | apple  |
    | orange |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_TRUE(outcome.ok) << JoinErrors(outcome.errors);
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    for (const auto& row : outcome.feature.scenarios) {
        ASSERT_EQ(row.steps.size(), 2u);
        ASSERT_TRUE(std::holds_alternative<std::string>(row.steps[0].rawArgument));
        EXPECT_EQ(std::get<std::string>(row.steps[0].rawArgument), "shared template body");
        EXPECT_TRUE(std::holds_alternative<std::monostate>(row.steps[1].rawArgument));
    }
    EXPECT_EQ(outcome.feature.scenarios[0].steps[1].text, "I publish apple");
    EXPECT_EQ(outcome.feature.scenarios[1].steps[1].text, "I publish orange");
}

// ---------------------------------------------------------------------
// Multi-error accumulation (impl::ParseOutcome::errors): every test above
// this section only ever exercises ONE malformed construct per ".feature"
// text, and every one of them now positively asserts outcome.errors.size()
// == 1u to rule out an unintended cascade. The tests below instead
// deliberately combine SEVERAL genuinely independent structural errors,
// spread across all three recovery categories (point/intermediate/
// corruptive - see ParseOutcome's doc comment in bdd.hpp), in a single
// ".feature" text, to prove the parser really does keep going after each
// one and reports the whole set in one pass - neither stopping early nor
// swallowing/duplicating any of them.
// ---------------------------------------------------------------------

TEST(GherkinParser, AccumulatesSeveralIndependentStructuralErrorsInOnePass) {
    // Four deliberately unrelated malformed constructs, each belonging to
    // a different call site: an unsupported 'Rule:' line (POINT), a Data
    // Table with no preceding step at the very start of a Scenario
    // (INTERMEDIATE - the second pipe line must be silently skipped, not
    // reported again), an Examples: row cell-count mismatch in a Scenario
    // Outline (POINT, and must NOT cascade into "must have at least one
    // data row" - see HandleExamplesTableRow's row-mismatch branch), and a
    // Data Table row cell-count mismatch in a later, unrelated Scenario
    // (POINT). None of these four can plausibly influence any of the
    // others' outcome - the parser must report exactly four errors, no
    // more, no fewer.
    constexpr std::string_view text = R"FEATURE(
Feature: Several independent errors
  Rule: Not supported here

  Scenario: Has an orphaned table
    | name  |
    | apple |

  Scenario Outline: Templated <n>
    Given step <n>

  Examples:
    | n |
    | 1 | 2 |

  Scenario: Has a ragged table
    Given the following items
      | name | qty |
      | pear |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    ASSERT_EQ(outcome.errors.size(), 4u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors[0].find("'Rule:' is not supported"), std::string::npos) << outcome.errors[0];
    EXPECT_NE(outcome.errors[1].find("data table with no preceding step"), std::string::npos) << outcome.errors[1];
    EXPECT_NE(outcome.errors[2].find("cell(s)"), std::string::npos) << outcome.errors[2];
    EXPECT_NE(outcome.errors[3].find("data table row has 1 cell(s), expected 2 (from header)"), std::string::npos)
        << outcome.errors[3];
}

TEST(GherkinParser, UnclosedDocStringResyncsAtNextScenarioWithoutHidingThatScenariosOwnError) {
    // The textbook CORRUPTIVE case: an unclosed Doc String must not be
    // allowed to silently swallow every subsequent line as bogus Doc
    // String "content" all the way to EOF - doing so would hide any
    // genuinely independent error in a LATER Scenario entirely (a much
    // worse outcome than just missing the Doc String's own error). Instead
    // the parser must resync at the next block boundary (here, the
    // following 'Scenario:' line), report the Doc String as unclosed
    // against its OWN opening line, and then parse Scenario B normally -
    // including reporting ITS OWN, entirely unrelated Data Table row
    // mismatch. Exactly two errors are expected: one per Scenario, neither
    // one swallowing the other.
    constexpr std::string_view text = R"FEATURE(
Feature: Doc string resync
  Scenario: A
    Given the article body is:
      """
      never closed

  Scenario: B
    Given the following items
      | col1 | col2 |
      | onecell |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    ASSERT_EQ(outcome.errors.size(), 2u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors[0].find("doc string is not closed"), std::string::npos) << outcome.errors[0];
    EXPECT_NE(outcome.errors[1].find("data table row has 1 cell(s), expected 2 (from header)"), std::string::npos)
        << outcome.errors[1];
    // Scenario B was parsed as its own, independent Scenario (not silently
    // absorbed as Doc String content) - the resync worked.
    ASSERT_EQ(outcome.feature.scenarios.size(), 2u);
    EXPECT_EQ(outcome.feature.scenarios[1].name, "B");
}

TEST(GherkinParser, MalformedDataTableWithNoPrecedingStepSkipsOnlyItsOwnContiguousRowsWithoutSpammingOneErrorPerRow) {
    // INTERMEDIATE recovery: a malformed table (here, one with no
    // preceding step to attach to) must be recorded ONCE, with every
    // remaining contiguous '|'-prefixed line of that SAME table silently
    // skipped - never one error per row - and normal parsing must resume
    // cleanly at the first non-table line afterwards (proven here by the
    // trailing step successfully attaching to the Scenario).
    constexpr std::string_view text = R"FEATURE(
Feature: Malformed table with no preceding step
  Scenario: A
    | x | y |
    | 1 | 2 |
    | 3 | 4 |
  Given a step right after the malformed table
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    ASSERT_EQ(outcome.errors.size(), 1u) << JoinErrors(outcome.errors);
    EXPECT_NE(outcome.errors.front().find("data table with no preceding step"), std::string::npos)
        << outcome.errors.front();
    // Recovery resumed cleanly: the step after the malformed table's three
    // rows attached normally, proving the parser is back in a sane state
    // rather than still confused by the skipped table.
    ASSERT_EQ(outcome.feature.scenarios.size(), 1u);
    ASSERT_EQ(outcome.feature.scenarios[0].steps.size(), 1u);
    EXPECT_EQ(outcome.feature.scenarios[0].steps[0].text, "a step right after the malformed table");
}

// ---------------------------------------------------------------------
// @timeout annotations (impl::ParseScenarioExecutionPolicy): reuses the
// existing @token tag grammar completely unchanged (no parser change - the
// tag is stored as an ordinary "timeout:5s" string, just like any other
// tag), so it is tested directly against effectiveTags (i.e. what
// impl::UnionTags would have already produced), the same way
// TagsAreSubsetOf()/JoinTagsForLabel() would be tested in isolation, rather
// than only indirectly through a full ParseFeatureText() + RunFeature()
// round trip. test_Gherkin_Integration.cpp separately covers the
// end-to-end enforcement behavior (a step function that actually blocks
// past the deadline, After hooks still running, etc).
// ---------------------------------------------------------------------

TEST(GherkinTimeoutPolicy, NoTimeoutTagLeavesPolicyCompletelyUnset) {
    const GherkinImpl::ScenarioExecutionPolicy policy =
        GherkinImpl::ParseScenarioExecutionPolicy({ "smoke", "slow" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_TRUE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, ValidSecondsSuffixParsesToMilliseconds) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:5s" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(5000));
}

TEST(GherkinTimeoutPolicy, ValidMillisecondsSuffixParsesToMilliseconds) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:500ms" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(500));
}

TEST(GherkinTimeoutPolicy, ValidMinutesSuffixParsesToMilliseconds) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:2m" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(120000));
}

TEST(GherkinTimeoutPolicy, BareUnitlessNumberIsRejectedNotDefaultedToSeconds) {
    // Deliberate design choice: an explicit unit suffix is REQUIRED to avoid
    // any ambiguity about what a bare number means; see
    // ParseTimeoutValue()'s comment in bdd.hpp.
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:5" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, NonNumericValueIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:abcs" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, MissingUnitSuffixIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

// Regression coverage: impl::ParseTimeoutValue's per-suffix-candidate loop
// tries "ms", "s", then "m" in turn, skipping (not erroring on) any
// candidate whose numberPart is empty ("no digits before the unit suffix;
// not a match after all" - see its own comment). "ms" alone is a value that
// disqualifies BOTH the "ms" candidate (numberPart "" - empty) AND the "s"
// candidate (numberPart "m" - non-numeric), so it exercises both of those
// per-candidate skip branches before finally falling through to the
// generic malformed-value error.
TEST(GherkinTimeoutPolicy, UnitSuffixWithNoDigitsIsRejectedNotMisreadAsAnotherSuffix) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:ms" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
    EXPECT_NE(policy.parseError.find("malformed @timeout value"), std::string::npos) << policy.parseError;
}

TEST(GherkinTimeoutPolicy, UnrecognizedUnitSuffixIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:5h" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, OverflowingValueIsRejectedInsteadOfWrappingViaUndefinedBehavior) {
    // A value that fits comfortably in an int64_t on its own, but whose
    // conversion to milliseconds (parsedValue * unit.msPerUnit) would
    // overflow int64_t - impl::ParseTimeoutValue must reject this as
    // malformed rather than performing the multiplication (UB on signed
    // overflow).
    const GherkinImpl::ScenarioExecutionPolicy policy =
        GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:200000000000000m" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
    EXPECT_NE(policy.parseError.find("too large"), std::string::npos) << policy.parseError;
}

TEST(GherkinTimeoutPolicy, ZeroDurationIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:0s" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, NegativeDurationIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:-5s" });

    EXPECT_FALSE(policy.timeout.has_value());
    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinTimeoutPolicy, ScenarioLevelTimeoutOverridesInheritedFeatureLevelTimeout) {
    // effectiveTags mirrors impl::UnionTags's real order: Feature tags
    // first, then Scenario tags appended - so the Scenario's own @timeout
    // (the more specific one) is expected to win over the inherited
    // Feature-level one.
    const std::vector<std::string> effectiveTags =
        GherkinImpl::UnionTags(/*featureTags=*/{ "timeout:30s" }, /*scenarioTags=*/{ "timeout:5s" });

    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy(effectiveTags);

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(5000));
}

TEST(GherkinTimeoutPolicy, FeatureLevelTimeoutAppliesWhenScenarioSetsNone) {
    const std::vector<std::string> effectiveTags =
        GherkinImpl::UnionTags(/*featureTags=*/{ "timeout:30s" }, /*scenarioTags=*/{ "smoke" });

    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy(effectiveTags);

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(30000));
}

// ---------------------------------------------------------------------
// @retry:N annotations (impl::ParseScenarioExecutionPolicy /
// impl::ParseRetryValue): Feature 6, retry/flaky annotations. Reuses the
// existing @token tag grammar completely unchanged (no parser change - the
// tag is stored as an ordinary "retry:3" string, just like "timeout:5s"),
// so - mirroring the GherkinTimeoutPolicy suite above - it is tested
// directly against effectiveTags rather than only indirectly through a
// full ParseFeatureText() + RunFeature() round trip.
// test_Gherkin_Integration.cpp separately covers the end-to-end retry
// enforcement behavior (a step that actually fails N-1 times then
// succeeds, onFailure invoked only for the final outcome, hooks re-running
// per attempt, @timeout+@retry interaction, parallel dispatch, etc).
// ---------------------------------------------------------------------

TEST(GherkinRetryPolicy, NoRetryTagLeavesMaxAttemptsAtDefaultOfOne) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "smoke", "slow" });

    EXPECT_EQ(policy.maxAttempts, 1u);
    EXPECT_TRUE(policy.parseError.empty());
}

TEST(GherkinRetryPolicy, ValidRetryValueParsesToMaxAttempts) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:3" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    EXPECT_EQ(policy.maxAttempts, 3u);
}

TEST(GherkinRetryPolicy, RetryOneIsAcceptedAndMeansNoRetry) {
    // "@retry:1" is a degenerate-but-valid case: it means "run once, no
    // retry" - exactly the same as omitting the tag entirely.
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:1" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    EXPECT_EQ(policy.maxAttempts, 1u);
}

TEST(GherkinRetryPolicy, ZeroIsRejectedNTotalAttemptsCannotBeZero) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:0" });

    EXPECT_FALSE(policy.parseError.empty());
    EXPECT_EQ(policy.parseErrorLabel, "Retry");
}

TEST(GherkinRetryPolicy, NegativeValueIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:-1" });

    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinRetryPolicy, NonNumericValueIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:abc" });

    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinRetryPolicy, EmptyValueIsRejected) {
    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy({ "retry:" });

    EXPECT_FALSE(policy.parseError.empty());
}

TEST(GherkinRetryPolicy, ScenarioLevelRetryOverridesInheritedFeatureLevelRetry) {
    // effectiveTags mirrors impl::UnionTags's real order: Feature tags
    // first, then Scenario tags appended - so the Scenario's own @retry
    // (the more specific one) is expected to win over the inherited
    // Feature-level one, exactly like GherkinTimeoutPolicy's analogous test.
    const std::vector<std::string> effectiveTags =
        GherkinImpl::UnionTags(/*featureTags=*/{ "retry:5" }, /*scenarioTags=*/{ "retry:2" });

    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy(effectiveTags);

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    EXPECT_EQ(policy.maxAttempts, 2u);
}

TEST(GherkinRetryPolicy, FeatureLevelRetryAppliesWhenScenarioSetsNone) {
    const std::vector<std::string> effectiveTags =
        GherkinImpl::UnionTags(/*featureTags=*/{ "retry:5" }, /*scenarioTags=*/{ "smoke" });

    const GherkinImpl::ScenarioExecutionPolicy policy = GherkinImpl::ParseScenarioExecutionPolicy(effectiveTags);

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    EXPECT_EQ(policy.maxAttempts, 5u);
}

TEST(GherkinRetryPolicy, RetryAndTimeoutCoexistIndependently) {
    const GherkinImpl::ScenarioExecutionPolicy policy =
        GherkinImpl::ParseScenarioExecutionPolicy({ "timeout:5s", "retry:3" });

    ASSERT_TRUE(policy.parseError.empty()) << policy.parseError;
    ASSERT_TRUE(policy.timeout.has_value());
    EXPECT_EQ(*policy.timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(policy.maxAttempts, 3u);
}

// ---------------------------------------------------------------------
// Cucumber-expression-lite pattern compiler (impl::CompileStepPattern):
// placeholder capture, literal-text escaping, and anchoring - tested
// directly against std::regex_match, independent of StepRegistry/
// MakeStepThunk's type conversion layer.
// ---------------------------------------------------------------------

TEST(GherkinCucumberExpression, CompilesIntPlaceholderAndCapturesOptionalSign) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("I have {int} apples");
    EXPECT_EQ(compiled.placeholderCount, 1u);

    std::smatch match;
    const std::string positive = "I have 3 apples";
    ASSERT_TRUE(std::regex_match(positive, match, compiled.regex));
    EXPECT_EQ(match[1].str(), "3");

    const std::string negative = "I have -7 apples";
    ASSERT_TRUE(std::regex_match(negative, match, compiled.regex));
    EXPECT_EQ(match[1].str(), "-7");
}

TEST(GherkinCucumberExpression, CompilesFloatPlaceholder) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("the ratio is {float}");
    std::smatch match;
    const std::string text = "the ratio is -2.5";
    ASSERT_TRUE(std::regex_match(text, match, compiled.regex));
    EXPECT_EQ(match[1].str(), "-2.5");
}

TEST(GherkinCucumberExpression, CompilesStringPlaceholderRequiringLiteralQuotes) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("labeled {string}");
    std::smatch match;
    const std::string text = R"(labeled "fresh fruit")";
    ASSERT_TRUE(std::regex_match(text, match, compiled.regex));
    // The captured group must be the content WITHOUT the surrounding quotes.
    EXPECT_EQ(match[1].str(), "fresh fruit");

    // Unquoted text must not match: {string} requires literal quote marks.
    const std::string unquoted = "labeled fresh fruit";
    EXPECT_FALSE(std::regex_match(unquoted, match, compiled.regex));
}

TEST(GherkinCucumberExpression, CompilesWordPlaceholderRejectingWhitespace) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("tagged {word}");
    std::smatch match;
    const std::string text = "tagged batch42";
    ASSERT_TRUE(std::regex_match(text, match, compiled.regex));
    EXPECT_EQ(match[1].str(), "batch42");

    // {word} is \S+ - a space inside it must break the (anchored) match.
    const std::string withSpace = "tagged batch 42";
    EXPECT_FALSE(std::regex_match(withSpace, match, compiled.regex));
}

TEST(GherkinCucumberExpression, PatternIsAnchoredNotJustAPrefixOrSubstringMatch) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("I have {int} apples");
    std::smatch match;

    // Trailing extra text must not be silently accepted.
    const std::string trailingExtra = "I have 3 apples and more";
    EXPECT_FALSE(std::regex_match(trailingExtra, match, compiled.regex));

    // Leading extra text must not be silently accepted either.
    const std::string leadingExtra = "well, I have 3 apples";
    EXPECT_FALSE(std::regex_match(leadingExtra, match, compiled.regex));
}

TEST(GherkinCucumberExpression, EscapesRegexMetacharactersInLiteralText) {
    const GherkinImpl::CompiledStepPattern compiled =
        GherkinImpl::CompileStepPattern("cost is $5.00 (approx) [tax incl.]");
    std::smatch match;

    const std::string exact = "cost is $5.00 (approx) [tax incl.]";
    EXPECT_TRUE(std::regex_match(exact, match, compiled.regex));

    // If '.', '(', ')', '[', ']' were NOT escaped, this decoy (each
    // metacharacter-adjacent literal character swapped, but shaped so an
    // UNESCAPED reading of the pattern -- '.' as "any char", "(approx)" as
    // a capture group of a "pprox" of any single leading char, "[tax
    // incl.]" as a character class -- would still accept it) would
    // incorrectly match too. With correct escaping it must not.
    const std::string decoy = "cost is $5X00 Xxpproxy t";
    EXPECT_FALSE(std::regex_match(decoy, match, compiled.regex));
}

// Regression/edge case: an unclosed '{' (no matching '}' anywhere after it)
// is NOT a registration-time error - impl::CompileStepPattern falls back to
// treating everything from the unclosed '{' onward as escaped literal text
// (see its own comment: "regexStr += EscapeRegexLiteral(pattern.substr(index)); break;").
// A pattern is therefore never rejected for a missing '}'; it just stops
// looking for any further placeholders once it can't find one.
TEST(GherkinCucumberExpression, UnclosedPlaceholderBraceFallsBackToLiteralTextInstead) {
    const GherkinImpl::CompiledStepPattern compiled = GherkinImpl::CompileStepPattern("I have {int apples");
    EXPECT_EQ(compiled.placeholderCount, 0u);

    std::smatch match;
    // The literal (escaped) text "{int apples" must match itself exactly.
    const std::string literal = "I have {int apples";
    EXPECT_TRUE(std::regex_match(literal, match, compiled.regex));

    // The "real" {int} placeholder behavior must NOT have kicked in.
    const std::string looksLikeAPlaceholder = "I have 3 apples";
    EXPECT_FALSE(std::regex_match(looksLikeAPlaceholder, match, compiled.regex));
}

TEST(GherkinCucumberExpression, UnknownPlaceholderNameThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::CompileStepPattern("a {money} amount"); }, std::invalid_argument);
}

// ---------------------------------------------------------------------
// Feature 7: tag EXPRESSIONS ("@smoke and not @slow") for Before/After
// hooks - a parallel, more expressive alternative to the original
// vector-of-tags AND/subset match (GherkinTagHelpers.TagsAreSubsetOfIsAndNotOr
// in test_Gherkin_Integration.cpp covers that original mechanism; it is
// completely untouched by any of this). Tested here, in isolation, straight
// against GherkinImpl::ParseTagExpression/EvaluateTagExpression - exactly
// the same "call impl:: directly, no RunFeature/death-test needed for
// registration-time errors" approach GherkinCucumberExpression above already
// uses for CompileStepPattern. End-to-end hook-matching through RunFeature()
// is covered separately in test_Gherkin_Integration.cpp.
// ---------------------------------------------------------------------

TEST(GherkinTagExpression, ParsesSingleTagAndEvaluatesMembership) {
    const auto expr = GherkinImpl::ParseTagExpression("@smoke");
    ASSERT_NE(expr, nullptr);
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"smoke"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {"other"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {}));
}

TEST(GherkinTagExpression, ParsesAndOperatorRequiringBothTags) {
    const auto expr = GherkinImpl::ParseTagExpression("@a and @b");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a", "b"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {"a"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {"b"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {}));
}

TEST(GherkinTagExpression, ParsesOrOperatorRequiringEitherTag) {
    const auto expr = GherkinImpl::ParseTagExpression("@a or @b");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a"}));
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"b"}));
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a", "b"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {}));
}

TEST(GherkinTagExpression, ParsesNotOperatorNegatingItsOperand) {
    const auto expr = GherkinImpl::ParseTagExpression("not @wip");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {}));
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"other"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {"wip"}));
}

TEST(GherkinTagExpression, ParenthesesOverrideDefaultPrecedence) {
    // Without parens, "and" binds tighter than "or" (see the precedence
    // test below), so "@a or @b and not @c" is NOT the same as this. Here
    // explicit parens force "(@a or @b) and (not @c)".
    const auto expr = GherkinImpl::ParseTagExpression("(@a or @b) and not @c");
    // a present, c present: (true or false) and (not true) = true and false = false.
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {"a", "c"}));
    // a present, c absent: (true or false) and (not false) = true and true = true.
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a"}));
    // neither a nor b present: (false or false) and ... = false, regardless of c.
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {}));
}

// Grammar precedence (highest to lowest): not > and > or. So
// "@a or @b and @c" must group as "@a or (@b and @c)", NOT "(@a or @b) and
// @c" - these two groupings disagree on tags={"a"} (b/c both absent):
//   correct:   @a or (@b and @c)  = true or (false and false)  = true
//   incorrect: (@a or @b) and @c  = (true or false) and false  = false
// so evaluating against exactly {"a"} distinguishes them unambiguously.
// The AST shape is also asserted directly, as a second, structural proof.
TEST(GherkinTagExpression, OperatorPrecedenceAndBindsTighterThanOr) {
    const auto expr = GherkinImpl::ParseTagExpression("@a or @b and @c");

    ASSERT_EQ(expr->op, GherkinImpl::TagExprOp::Or);
    ASSERT_NE(expr->left, nullptr);
    ASSERT_NE(expr->right, nullptr);
    EXPECT_EQ(expr->left->op, GherkinImpl::TagExprOp::Tag);
    EXPECT_EQ(expr->left->tagName, "a");
    EXPECT_EQ(expr->right->op, GherkinImpl::TagExprOp::And);
    ASSERT_NE(expr->right->left, nullptr);
    ASSERT_NE(expr->right->right, nullptr);
    EXPECT_EQ(expr->right->left->tagName, "b");
    EXPECT_EQ(expr->right->right->tagName, "c");

    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a"}))
        << "'@a or @b and @c' must group as '@a or (@b and @c)' (and > or)";
}

TEST(GherkinTagExpression, DoubleNotIsAllowedAndCancelsOut) {
    const auto expr = GherkinImpl::ParseTagExpression("not not @a");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*expr, {"a"}));
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(*expr, {}));
}

TEST(GherkinTagExpression, KeywordsAreCaseInsensitive) {
    const auto mixedAnd = GherkinImpl::ParseTagExpression("@a AND @b");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*mixedAnd, {"a", "b"}));

    const auto mixedOr = GherkinImpl::ParseTagExpression("@a Or @b");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*mixedOr, {"b"}));

    const auto mixedNot = GherkinImpl::ParseTagExpression("NOT @a");
    EXPECT_TRUE(GherkinImpl::EvaluateTagExpression(*mixedNot, {}));
}

TEST(GherkinTagExpression, EmptyExpressionThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression(""); }, std::invalid_argument);
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("   "); }, std::invalid_argument);
}

TEST(GherkinTagExpression, UnbalancedParensThrowInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("(@a"); }, std::invalid_argument) << "missing ')'";
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@a)"); }, std::invalid_argument) << "unexpected trailing ')'";
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("((@a)"); }, std::invalid_argument) << "missing outer ')'";
}

TEST(GherkinTagExpression, MissingOperandThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@a and"); }, std::invalid_argument);
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("or @a"); }, std::invalid_argument);
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("not"); }, std::invalid_argument);
}

TEST(GherkinTagExpression, UnknownKeywordThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@a xor @b"); }, std::invalid_argument);
}

TEST(GherkinTagExpression, TagWithoutLeadingAtSignThrowsInvalidArgument) {
    // "a" (no '@') tokenizes as an identifier, which is only ever valid as
    // one of the "and"/"or"/"not" keywords - any other bare word (a tag
    // written without its required leading '@') is therefore rejected the
    // same way an unrecognized keyword is.
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("a and b"); }, std::invalid_argument);
}

TEST(GherkinTagExpression, BareAtSignWithNoTagNameThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@"); }, std::invalid_argument);
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@ and @b"); }, std::invalid_argument);
}

// A character that is neither whitespace, '(', ')', '@', nor a keyword-
// alphabetic character (see impl::TokenizeTagExpression's final "else"
// branch) is rejected immediately, independent of the '@'-tag and
// unrecognized-keyword cases above.
TEST(GherkinTagExpression, UnrecognizedCharacterThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@a & @b"); }, std::invalid_argument);
    EXPECT_THROW({ GherkinImpl::ParseTagExpression("@a $ @b"); }, std::invalid_argument);
}

// Every TagExpressionNode ParseTagExpression ever produces has op set to one
// of the 4 real enumerators - EvaluateTagExpression's switch is genuinely
// exhaustive for any node this parser can build. A corrupted op value
// (unreachable via ParseTagExpression itself) is the only way to fall
// through to the trailing "return false" EvaluateTagExpression's own comment
// documents as unreachable in practice.
TEST(GherkinTagExpression, InvalidOpValueFallsThroughToUnreachableReturnFalse) {
    const GherkinImpl::TagExpressionNode node{
        .op = static_cast<GherkinImpl::TagExprOp>(99), .tagName = "", .left = nullptr, .right = nullptr };
    EXPECT_FALSE(GherkinImpl::EvaluateTagExpression(node, {}));
}

// ---------------------------------------------------------------------
// StepRegistry::AddBeforeHookExpr/AddAfterHookExpr - registration-time
// parsing (a malformed expression must throw immediately from the call
// itself, mirroring RegisteringUnknownPlaceholderNameThrows above for
// RegisterGiven/CompileStepPattern - never deferred to RunFeature()).
// Full end-to-end hook-matching through RunFeature() is covered in
// test_Gherkin_Integration.cpp, not here (this file stays isolated from
// the StepRegistry/RunFeature execution machinery - see the file banner).
// ---------------------------------------------------------------------

TEST(GherkinStepRegistryHookExpr, AddBeforeHookExprRegistersSuccessfullyWithValidExpression) {
    BabyBehave::BDD::Gherkin::StepRegistry registry;
    EXPECT_NO_THROW({ registry.AddBeforeHookExpr("@smoke and not @slow", [](BabyBehave::BDD::TestContext&) {}); });
    EXPECT_EQ(registry.BeforeHooks().size(), 1u);
    EXPECT_EQ(registry.BeforeHooks().front().label, "@smoke and not @slow");
    EXPECT_TRUE(registry.BeforeHooks().front().tags.empty());
    EXPECT_TRUE(registry.BeforeHooks().front().expression.has_value());
}

TEST(GherkinStepRegistryHookExpr, AddAfterHookExprRegistersSuccessfullyWithValidExpression) {
    BabyBehave::BDD::Gherkin::StepRegistry registry;
    EXPECT_NO_THROW({ registry.AddAfterHookExpr("@vip or @premium", [](BabyBehave::BDD::TestContext&) {}); });
    EXPECT_EQ(registry.AfterHooks().size(), 1u);
    EXPECT_EQ(registry.AfterHooks().front().label, "@vip or @premium");
    EXPECT_TRUE(registry.AfterHooks().front().tags.empty());
    EXPECT_TRUE(registry.AfterHooks().front().expression.has_value());
}

TEST(GherkinStepRegistryHookExpr, AddBeforeHookExprThrowsImmediatelyOnMalformedExpression) {
    BabyBehave::BDD::Gherkin::StepRegistry registry;
    EXPECT_THROW({ registry.AddBeforeHookExpr("not (@wip or @skip", [](BabyBehave::BDD::TestContext&) {}); },
                 std::invalid_argument);
    // The failed registration must not have left a partial hook behind.
    EXPECT_TRUE(registry.BeforeHooks().empty());
}

TEST(GherkinStepRegistryHookExpr, AddAfterHookExprThrowsImmediatelyOnMalformedExpression) {
    BabyBehave::BDD::Gherkin::StepRegistry registry;
    EXPECT_THROW({ registry.AddAfterHookExpr("", [](BabyBehave::BDD::TestContext&) {}); }, std::invalid_argument);
    EXPECT_TRUE(registry.AfterHooks().empty());
}

// ---------------------------------------------------------------------
// Internal defensive guards (v0.9.1 100%-coverage closure): a handful of
// impl::-level branches this codebase's own comments already document as
// "unreachable in practice" - defensive invariant-guards kept only so the
// surrounding code compiles/never dereferences something empty, never
// actually reachable through StepRegistry::AddStepDefinition's registration-
// time validation or ParseFeatureText's own state machine. Called here
// directly with deliberately invariant-violating state, the same "call
// impl:: directly, no RunFeature/death-test needed" convention this file
// already uses throughout (see e.g. GherkinCucumberExpression/
// GherkinTagExpression above) - a white-box test is the ONLY way to
// exercise these at all, since the normal registration/parsing flow
// structurally cannot produce the state they guard against.
// ---------------------------------------------------------------------

// impl::InvokeStepNoRawArg's "captured argument count does not match step
// definition" branch: unreachable via AddStepDefinition/TryMatch, which
// always keep captures.size() and paramCount in lockstep whenever
// rawArgKind == RawArgumentKind::None (AddStepDefinition throws at
// registration time otherwise - see StepDefinitionRawArgKind's doc comment).
TEST(GherkinInternalDefensiveGuards, InvokeStepNoRawArgCapturedArgumentCountMismatchReturnsFalse) {
    auto stepFn = [](BabyBehave::BDD::TestContext&, int) -> bool { return true; };
    using F = decltype(stepFn);
    using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
    BabyBehave::BDD::TestContext ctx;
    const std::vector<std::string> captures; // size 0, but paramCount is 1.
    EXPECT_FALSE((GherkinImpl::InvokeStepNoRawArg<ArgsTuple, 1, false>(stepFn, ctx, captures)));
}

// impl::InvokeStepWithRawArg's two "captured argument count does not match
// step definition" branches (DataTable-sink and std::string-sink) - same
// unreachability reasoning as above, this time for rawArgKind != None (where
// StepDefinitionRawArgKind guarantees capturesN == compiled.placeholderCount).
TEST(GherkinInternalDefensiveGuards, InvokeStepWithRawArgDataTableCapturedArgumentCountMismatchReturnsFalse) {
    auto stepFn = [](BabyBehave::BDD::TestContext&, int, const BabyBehave::BDD::Gherkin::DataTable&) -> bool { return true; };
    using F = decltype(stepFn);
    using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
    BabyBehave::BDD::TestContext ctx;
    const std::vector<std::string> captures; // size 0, but capturesN (paramCount - 1) is 1.
    const GherkinImpl::RawArgument rawArgument{};
    EXPECT_FALSE((GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 2>(stepFn, ctx, captures, rawArgument)));
}

TEST(GherkinInternalDefensiveGuards, InvokeStepWithRawArgDocStringCapturedArgumentCountMismatchReturnsFalse) {
    auto stepFn = [](BabyBehave::BDD::TestContext&, int, const std::string&) -> bool { return true; };
    using F = decltype(stepFn);
    using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
    BabyBehave::BDD::TestContext ctx;
    const std::vector<std::string> captures; // size 0, but capturesN (paramCount - 1) is 1.
    const GherkinImpl::RawArgument rawArgument{};
    EXPECT_FALSE((GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 2>(stepFn, ctx, captures, rawArgument)));
}

// impl::InvokeStepWithRawArg's final "not a recognized raw-argument type"
// arm: StepDefinitionRawArgKind only ever returns a non-None kind when F's
// real trailing parameter is DataTable or std::string (else it throws at
// registration time), so InvokeStepWithRawArg is never actually CALLED for
// an F like this one through AddStepDefinition/MakeStepThunk - even though
// it IS instantiated (MakeStepThunk's lambda body references both branches
// unconditionally for every F). This is an ordinary placeholder-only step
// definition (last parameter is plain int), used all over this codebase;
// this test just calls its already-compiled InvokeStepWithRawArg
// instantiation directly instead of through that lambda.
TEST(GherkinInternalDefensiveGuards, InvokeStepWithRawArgUnrecognizedTrailingParameterTypeReturnsFalse) {
    auto stepFn = [](BabyBehave::BDD::TestContext&, int) -> bool { return true; };
    using F = decltype(stepFn);
    using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
    BabyBehave::BDD::TestContext ctx;
    const std::vector<std::string> captures{ "5" };
    const GherkinImpl::RawArgument rawArgument{};
    EXPECT_FALSE((GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 1>(stepFn, ctx, captures, rawArgument)));
}

// impl::InvokeStepWithRawArg's "paramCount == 0" else-branch: AddStepDefinition
// only ever computes a non-None rawArgKind when paramCount >= 1, so a
// zero-parameter step definition (very common - a Given/When/Then with no
// placeholder captures at all) always keeps rawArgKind == None and this
// runtime `if (rawArgKind == None) return InvokeStepNoRawArg...` branch in
// MakeStepThunk's lambda never even calls InvokeStepWithRawArg for it - but
// the function IS still instantiated (and compiled) for such an F, same
// reasoning as the test above.
TEST(GherkinInternalDefensiveGuards, InvokeStepWithRawArgZeroParamCountReturnsFalse) {
    auto stepFn = [](BabyBehave::BDD::TestContext&) -> bool { return true; };
    using F = decltype(stepFn);
    using ArgsTuple = GherkinImpl::CallableSignature<F>::ArgsTuple;
    BabyBehave::BDD::TestContext ctx;
    const std::vector<std::string> captures;
    const GherkinImpl::RawArgument rawArgument{};
    EXPECT_FALSE((GherkinImpl::InvokeStepWithRawArg<ArgsTuple, 0>(stepFn, ctx, captures, rawArgument)));
}

// impl::ResolveStepTarget's "no Scenario in progress" throw: unreachable via
// ProcessFeatureLine, which always resets lastStepTarget before
// currentScenario is ever swapped/flushed - so a StepTarget with
// inBackground == false is only ever produced while currentScenario is
// engaged.
TEST(GherkinInternalDefensiveGuards, ResolveStepTargetThrowsWhenNoScenarioInProgress) {
    GherkinImpl::FeatureParseState state;
    const GherkinImpl::StepTarget target{ .inBackground = false, .index = 0 };
    EXPECT_THROW(GherkinImpl::ResolveStepTarget(state, target), std::logic_error);
}

// impl::HandleDataTableLine's "data table with no preceding step" branch
// reached while state.inDataTable is already true: unreachable via
// ProcessFeatureLine, which only ever sets inDataTable true right after
// lastStepTarget is set (both cleared together on every context change).
TEST(GherkinInternalDefensiveGuards, HandleDataTableLineNoPrecedingStepWhileInDataTableRecordsErrorAndResets) {
    GherkinImpl::FeatureParseState state;
    state.inDataTable = true;
    state.lastStepTarget.reset();

    GherkinImpl::HandleDataTableLine(state, "| a | b |", 7);

    ASSERT_EQ(state.errors.size(), 1u) << JoinErrors(state.errors);
    EXPECT_NE(state.errors.front().find("data table with no preceding step"), std::string::npos) << state.errors.front();
    EXPECT_FALSE(state.inDataTable);
    EXPECT_TRUE(state.skipMalformedTableLines);
}

// impl::HandleExamplesTableRow's "!pendingExamples" auto-init guard:
// unreachable via ProcessFeatureLine, which only ever routes here while
// state.inExamplesTable is true, which is only ever set true together with
// state.pendingExamples being engaged (see the 'Examples:'/'Scenarios:' line
// handling).
TEST(GherkinInternalDefensiveGuards, HandleExamplesTableRowAutoInitializesMissingPendingExamples) {
    GherkinImpl::FeatureParseState state;
    state.pendingExamples.reset();

    GherkinImpl::HandleExamplesTableRow(state, "| a | b |", 3);

    ASSERT_TRUE(state.pendingExamples.has_value());
    EXPECT_EQ(state.pendingExamples->header, (std::vector<std::string>{ "a", "b" }));
    EXPECT_TRUE(state.haveExamplesHeader);
}

// impl::HandleDocStringLine's closing-delimiter "no preceding step" guard:
// unreachable via ProcessFeatureLine, since reaching a CLOSING '"""' with
// state.inDocString true implies the OPENING '"""' already validated
// state.lastStepTarget (the only place state.inDocString is ever set true).
TEST(GherkinInternalDefensiveGuards, HandleDocStringLineClosingWithNoPrecedingStepRecordsErrorAndResets) {
    GherkinImpl::FeatureParseState state;
    state.inDocString = true;
    state.docStringLines = { "orphaned content" };
    state.lastStepTarget.reset();

    GherkinImpl::HandleDocStringLine(state, R"(""")", R"(""")", 9);

    ASSERT_EQ(state.errors.size(), 1u) << JoinErrors(state.errors);
    EXPECT_NE(state.errors.front().find("doc string with no preceding step"), std::string::npos) << state.errors.front();
    EXPECT_FALSE(state.inDocString);
    EXPECT_TRUE(state.docStringLines.empty());
}
