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

#include <regex>
#include <string>
#include <string_view>

namespace GherkinImpl = BabyBehave::BDD::Gherkin::impl;

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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;

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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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

    ASSERT_TRUE(outcome.ok) << outcome.error;
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
    EXPECT_NE(outcome.error.find("no 'Feature:' found"), std::string::npos) << outcome.error;
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
    EXPECT_NE(outcome.error.find("multiple 'Feature:' sections"), std::string::npos) << outcome.error;
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
    EXPECT_NE(outcome.error.find("step found outside of a Background:/Scenario:"), std::string::npos) << outcome.error;
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
    EXPECT_NE(outcome.error.find("'Rule:' is not supported"), std::string::npos) << outcome.error;
}

TEST(GherkinParser, RejectsScenarioOutlineAndExamples) {
    constexpr std::string_view outlineText = R"FEATURE(
Feature: Outline
  Scenario Outline: Templated
    Given a <thing>
)FEATURE";
    const GherkinImpl::ParseOutcome outlineOutcome = GherkinImpl::ParseFeatureText(outlineText);
    ASSERT_FALSE(outlineOutcome.ok);
    EXPECT_NE(outlineOutcome.error.find("Scenario Outline/Examples are not supported"), std::string::npos) << outlineOutcome.error;

    constexpr std::string_view examplesText = R"FEATURE(
Feature: Examples table
  Scenario: A
    Given a step
  Examples:
    | thing |
)FEATURE";
    const GherkinImpl::ParseOutcome examplesOutcome = GherkinImpl::ParseFeatureText(examplesText);
    ASSERT_FALSE(examplesOutcome.ok);
    EXPECT_NE(examplesOutcome.error.find("Scenario Outline/Examples are not supported"), std::string::npos) << examplesOutcome.error;
}

TEST(GherkinParser, RejectsScenarioTemplateAndScenariosKeyword) {
    constexpr std::string_view templateText = R"FEATURE(
Feature: Template synonym
  Scenario Template: Templated
    Given a <thing>
)FEATURE";
    const GherkinImpl::ParseOutcome templateOutcome = GherkinImpl::ParseFeatureText(templateText);
    ASSERT_FALSE(templateOutcome.ok);
    EXPECT_NE(templateOutcome.error.find("Scenario Outline/Examples are not supported"), std::string::npos) << templateOutcome.error;

    constexpr std::string_view scenariosText = R"FEATURE(
Feature: Scenarios synonym
  Scenario: A
    Given a step
  Scenarios:
    | thing |
)FEATURE";
    const GherkinImpl::ParseOutcome scenariosOutcome = GherkinImpl::ParseFeatureText(scenariosText);
    ASSERT_FALSE(scenariosOutcome.ok);
    EXPECT_NE(scenariosOutcome.error.find("Scenario Outline/Examples are not supported"), std::string::npos) << scenariosOutcome.error;
}

TEST(GherkinParser, RejectsDocStrings) {
    constexpr std::string_view text = R"FEATURE(
Feature: Doc strings
  Scenario: A
    Given a step with a doc string
    """
    some free-form text
    """
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    EXPECT_NE(outcome.error.find("Doc strings are not supported"), std::string::npos) << outcome.error;
}

TEST(GherkinParser, RejectsDataTables) {
    constexpr std::string_view text = R"FEATURE(
Feature: Data tables
  Scenario: A
    Given the following items
      | name  | qty |
      | apple | 3   |
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    EXPECT_NE(outcome.error.find("Data tables are not supported"), std::string::npos) << outcome.error;
}

TEST(GherkinParser, ParseErrorsIncludeTheRealLineNumber) {
    // "Rule:" is on line 3 (1: "", 2: "Feature: ...", 3: "  Rule: ...").
    constexpr std::string_view text = R"FEATURE(
Feature: Line-numbered error
  Rule: Some rule
)FEATURE";

    const GherkinImpl::ParseOutcome outcome = GherkinImpl::ParseFeatureText(text);

    ASSERT_FALSE(outcome.ok);
    EXPECT_NE(outcome.error.find("line 3:"), std::string::npos) << outcome.error;
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

TEST(GherkinCucumberExpression, UnknownPlaceholderNameThrowsInvalidArgument) {
    EXPECT_THROW({ GherkinImpl::CompileStepPattern("a {money} amount"); }, std::invalid_argument);
}
