#include <gtest/gtest.h>
#include <BabyBehave/bdd.hpp>

#include <memory>
#include <stdexcept>
#include <string>

using namespace BabyBehave::BDD;

namespace {

void SetupTrivialContext(TestContext&) {}

bool StepPreconditionTrue(TestContext&) { return true; }
bool StepThenTrue(TestContext&) { return true; }
bool StepFalseCondition(TestContext&) { return false; }
bool StepThrows(TestContext&) { throw std::runtime_error("boom"); }

// Two passing sub-checks, one failing (mirrors
// StepActionWithSoftChecks in tests/bdd/test_SelfTest.cpp).
bool StepActionSoftCheckMixed(TestContext& context) {
    SoftCheck checks(context);
    const int count = 15;
    checks.Check("has valid id", true);
    checks.Check("name matches", true);
    checks.Check("count in range", count >= 1 && count <= 10, "count was " + std::to_string(count));
    return checks.AllPassed();
}

// Every sub-check passes; the step itself must therefore pass too.
bool StepActionSoftCheckAllPass(TestContext& context) {
    SoftCheck checks(context);
    checks.Check("has valid id", true);
    checks.Check("name matches", true, "should never surface anywhere");
    return checks.AllPassed();
}

// TWO failing sub-checks (plus one passing one), so
// FormatFailedSoftChecks() has to join more than one failed label with
// "; " rather than emitting just a single label.
bool StepActionSoftCheckTwoFailures(TestContext& context) {
    SoftCheck checks(context);
    checks.Check("has valid id", true);
    checks.Check("name matches", false, "expected Bob, got Alice");
    checks.Check("count in range", false, "count was 15");
    return checks.AllPassed();
}

} // namespace

// ---------------------------------------------------------------------
// SoftCheck, under SetCollectFailuresMode(true): a mix of passing and
// failing sub-checks. The failing step's StepResult::message must be
// augmented with the failing sub-check's label/message; passing labels
// must not appear.
// ---------------------------------------------------------------------
TEST(BabyBehaveTestSoftCheck, MixedSoftChecksUnderCollectFailuresMode) {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)
        .When(StepActionSoftCheckMixed)
        .Then(StepThenTrue);

    const TestResult& result = test.Execute();

    ASSERT_EQ(result.steps.size(), 3u);
    EXPECT_FALSE(result.allPassed);

    const auto& precondition = result.steps[0];
    EXPECT_TRUE(precondition.passed);
    EXPECT_TRUE(precondition.message.empty());

    const auto& action = result.steps[1];
    EXPECT_EQ(action.stepLabel, "Action");
    EXPECT_EQ(action.stepName, "StepActionSoftCheckMixed");
    EXPECT_FALSE(action.passed);
    EXPECT_EQ(action.message, "Action failed: count in range (count was 15)");
    EXPECT_EQ(action.message.find("has valid id"), std::string::npos);
    EXPECT_EQ(action.message.find("name matches"), std::string::npos);

    const auto& then = result.steps[2];
    EXPECT_TRUE(then.passed);
    EXPECT_TRUE(then.message.empty());
}

// ---------------------------------------------------------------------
// SoftCheck reaching the DEFAULT (non-collect-failures) callback path:
// the augmented message (with the failing sub-check's label/message)
// must reach SetOnConditionNotVerifiedCallback.
// ---------------------------------------------------------------------
TEST(BabyBehaveTestSoftCheck, SoftChecksReachDefaultCallback) {
    int conditionNotVerifiedCount = 0;
    std::string capturedMessage;
    {
        BabyBehaveTest test = GivenA(SetupTrivialContext);
        test.SetOnConditionNotVerifiedCallback([&](const std::string& msg) {
            ++conditionNotVerifiedCount;
            capturedMessage = msg;
        });
        test.With(StepPreconditionTrue)
            .When(StepActionSoftCheckMixed)
            .Then(StepThenTrue);
    } // ~BabyBehaveTest() runs Execute() here.

    EXPECT_EQ(conditionNotVerifiedCount, 1);
    EXPECT_TRUE(capturedMessage.starts_with("Action failed: count in range (count was 15)"));
}

// ---------------------------------------------------------------------
// SoftCheck where every sub-check passes: the step itself must pass and
// no soft-check text should be appended anywhere.
// ---------------------------------------------------------------------
TEST(BabyBehaveTestSoftCheck, AllSubChecksPassingMeansStepPasses) {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)
        .When(StepActionSoftCheckAllPass)
        .Then(StepThenTrue);

    const TestResult& result = test.Execute();

    ASSERT_EQ(result.steps.size(), 3u);
    EXPECT_TRUE(result.allPassed);
    for (const auto& step : result.steps) {
        EXPECT_TRUE(step.passed);
        EXPECT_TRUE(step.message.empty());
    }
}

// ---------------------------------------------------------------------
// SoftCheck with TWO failing sub-checks in the same step: their
// "label (message)" pairs must both appear in the augmented message,
// joined by "; ".
// ---------------------------------------------------------------------
TEST(BabyBehaveTestSoftCheck, TwoFailedSubChecksAreJoinedWithSemicolon) {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)
        .When(StepActionSoftCheckTwoFailures)
        .Then(StepThenTrue);

    const TestResult& result = test.Execute();

    ASSERT_EQ(result.steps.size(), 3u);
    const auto& action = result.steps[1];
    EXPECT_FALSE(action.passed);
    EXPECT_EQ(action.message,
              "Action failed: name matches (expected Bob, got Alice); count in range (count was 15)");
}

// ---------------------------------------------------------------------
// SetCollectFailuresMode(true) with a mix of a passing step, a failing
// step (condition false) and a step that throws: execution must
// continue past each failure, and every StepResult must reflect its own
// outcome. A thrown exception's message must be the raw exception
// message (no default-callback " (at file:line)" formatting, since that
// suffix is only added on the non-collect-failures callback path).
// ---------------------------------------------------------------------
TEST(BabyBehaveTestCollectFailures, MixedPassFailThrow) {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)  // passes
        .When(StepFalseCondition)     // fails
        .And(StepThrows)              // throws
        .Then(StepThenTrue);          // passes

    const TestResult& result = test.Execute();

    ASSERT_EQ(result.steps.size(), 4u);
    EXPECT_FALSE(result.allPassed);

    EXPECT_TRUE(result.steps[0].passed);
    EXPECT_TRUE(result.steps[0].message.empty());

    EXPECT_FALSE(result.steps[1].passed);
    EXPECT_EQ(result.steps[1].message, "Action failed");

    EXPECT_FALSE(result.steps[2].passed);
    EXPECT_EQ(result.steps[2].message, "boom");

    EXPECT_TRUE(result.steps[3].passed);
    EXPECT_TRUE(result.steps[3].message.empty());
}

// ---------------------------------------------------------------------
// SetCollectFailuresMode(true) where every step passes.
// ---------------------------------------------------------------------
TEST(BabyBehaveTestCollectFailures, AllStepsPass) {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)
        .When(StepPreconditionTrue) // reused, also just returns true
        .Then(StepThenTrue);

    const TestResult& result = test.Execute();

    ASSERT_EQ(result.steps.size(), 3u);
    EXPECT_TRUE(result.allPassed);
    for (const auto& step : result.steps) {
        EXPECT_TRUE(step.passed);
    }
}

// ---------------------------------------------------------------------
// Execute() is idempotent: a second explicit call must return the same
// cached result without re-running any step. Proven indirectly via a
// step that increments a shared counter each time it actually runs.
// ---------------------------------------------------------------------
TEST(BabyBehaveTestExecute, IsIdempotent) {
    auto counter = std::make_shared<int>(0);

    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With([counter](TestContext&) {
        ++(*counter);
        return true;
    });

    const TestResult& first = test.Execute();
    const std::string firstTestName = first.testName;
    const std::size_t firstStepCount = first.steps.size();
    const bool firstAllPassed = first.allPassed;

    const TestResult& second = test.Execute();

    EXPECT_EQ(second.testName, firstTestName);
    EXPECT_EQ(second.steps.size(), firstStepCount);
    EXPECT_EQ(second.allPassed, firstAllPassed);
    EXPECT_EQ(*counter, 1);
}
