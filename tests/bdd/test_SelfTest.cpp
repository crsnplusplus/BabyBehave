// SelfTest.cpp
//
// A "dogfood" example: BabyBehave testing BabyBehave, using its own
// Given/When/Then fluent macros.
//
// This is NOT a replacement for the gtest suite in tests/. It is an
// independent, complementary coverage source for
// include/BabyBehave/bdd.hpp: every scenario below drives the public API
// (GivenA/With/When/Then/And/Or/But, TestContext::Set/Get,
// SetOnConditionNotVerifiedCallback, SetOnExceptionCallback,
// SetCollectFailuresMode/Execute/GetResult, SoftCheck) and then checks -
// via plain C++ assertions in main() - that the library actually behaved
// the way its contract promises.
//
// IMPORTANT: BabyBehaveTest's default failure callbacks call
// std::exit(EXIT_FAILURE) on any failed condition or caught exception, and
// BabyBehaveTest::Execute() runs from the destructor. If we let any
// scenario use the defaults, the process would die on the very first
// failure and none of the later scenarios (several of which are
// *expected* to fail/throw, on purpose, to exercise those code paths)
// would ever run. So every scenario below installs its own
// SetOnConditionNotVerifiedCallback/SetOnExceptionCallback that RECORD the
// outcome instead of exiting, and each scenario's BabyBehaveTest lives in
// its own nested scope so Execute() (fired by the destructor) completes
// before we inspect what was recorded.

#include <BabyBehave/bdd.hpp>
#include <BabyBehave/reporters.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;

namespace {

// Accumulates the TestResult of every scenario below that opts into
// SetCollectFailuresMode(true) (see bdd.hpp) - i.e. every scenario that
// actually produces a complete, serializable StepResult list, per
// reporters.hpp's documented constraint. Fed to
// BabyBehave::BDD::Reporters::ToJUnitXml()/ToTap() at the very end of
// main() below, to demonstrate a realistic "accumulate results from
// multiple GivenA(...) scenarios, then emit ONE combined CI report at the
// end of the run" usage.
std::vector<TestResult> g_collectFailuresResults;

// Captures what the library reported instead of letting the default
// callbacks terminate the process.
struct CallbackRecorder {
    int conditionNotVerifiedCount = 0;
    std::string lastConditionMessage;

    int exceptionCount = 0;
    std::string lastExceptionStep;
    std::string lastExceptionMessage;

    void Wire(BabyBehaveTest& test) {
        test.SetOnConditionNotVerifiedCallback([this](const std::string& msg) {
            ++conditionNotVerifiedCount;
            lastConditionMessage = msg;
        });
        test.SetOnExceptionCallback([this](const std::string& step, const std::exception& e) {
            ++exceptionCount;
            lastExceptionStep = step;
            lastExceptionMessage = e.what();
        });
    }
};

void ReportScenario(const std::string& name, bool passed, int& passCount, int& totalCount) {
    ++totalCount;
    if (passed) {
        ++passCount;
    }
    std::cout << (passed ? "[OK]   " : "[FAIL] ") << name << '\n';
}

// ---------------------------------------------------------------------
// Scenario 1: fully happy path chain - With/When/Then/And/Or/But all
// succeed, so neither failure callback should fire.
// ---------------------------------------------------------------------

void SetupHappyContext(TestContext& context) {
    context.Set<int>("answer", 42);
}

bool StepPreconditionTrue(TestContext&) { return true; }
bool StepActionTrue(TestContext&) { return true; }
bool StepPostconditionAnswerIs42(TestContext& context) {
    return context.Get<int>("answer") == 42;
}
bool StepAndTrue(TestContext&) { return true; }
bool StepOrTrue(TestContext&) { return true; }
bool StepButTrue(TestContext&) { return true; }

bool RunHappyPathScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupHappyContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionAnswerIs42)
            .And(StepAndTrue)
            .Or(StepOrTrue)
            .But(StepButTrue);
    } // BabyBehaveTest destructs here -> Execute() runs.

    const bool asExpected = recorder.conditionNotVerifiedCount == 0 && recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  HappyPath: expected no callbacks, got conditionNotVerifiedCount="
                  << recorder.conditionNotVerifiedCount << " exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 2: an Action step returns false - onConditionNotVerified
// should fire exactly once, with a sensible message, and must not exit.
// ---------------------------------------------------------------------

void SetupTrivialContext(TestContext&) {}

bool StepActionFalse(TestContext&) { return false; }

bool RunFailingConditionScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionFalse)
            .Then(StepAndTrue /* trivially-true; keeps this scenario focused on the Action failure */);
    }

    // lastConditionMessage now carries a " (at file:line)" suffix pointing
    // at the .When(StepActionFalse) call a few lines above (when
    // __cpp_lib_source_location is available) - check the base message as
    // a prefix rather than requiring an exact match.
    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with("Action failed") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  FailingCondition: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                  << " lastConditionMessage=\"" << recorder.lastConditionMessage
                  << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 2b: a Precondition step returns false, all other steps
// succeed cleanly - exactly one condition-not-verified callback, zero
// exceptions.
// ---------------------------------------------------------------------

bool StepPreconditionFalse(TestContext&) { return false; }

bool RunFailingPreconditionScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionFalse)
            .When(StepActionTrue)
            .Then(StepAndTrue /* reused as a trivially-true postcondition */);
    }

    // See the FailingCondition scenario above re: the " (at file:line)"
    // suffix now appended to lastConditionMessage.
    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with("Precondition failed") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  FailingPrecondition: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                  << " lastConditionMessage=\"" << recorder.lastConditionMessage
                  << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 3: a step throws a std::exception - onException should fire
// with the right step label and the exception's message.
// ---------------------------------------------------------------------

bool StepActionThrowsStdException(TestContext&) {
    throw std::runtime_error("boom from action");
}

bool RunStdExceptionScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionThrowsStdException)
            .Then(StepAndTrue);
    }

    // lastExceptionStep stays exactly "Action" (the step-type label is
    // never touched), but lastExceptionMessage now carries a
    // " (at file:line)" suffix pointing at the .When(...) call a few lines
    // above (when __cpp_lib_source_location is available).
    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Action" &&
                             recorder.lastExceptionMessage.starts_with("boom from action") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  StdException: exceptionCount=" << recorder.exceptionCount << " step=\""
                  << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                  << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 4: a step throws something that is NOT a std::exception -
// exercises the catch(...) fallback path. Must still be caught safely,
// routed to onException with a generic message, and must not crash the
// process.
// ---------------------------------------------------------------------

bool StepPostconditionThrowsNonStdType(TestContext&) {
    throw 42; // deliberately not a std::exception
}

bool RunNonStdExceptionScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionThrowsNonStdType);
    }

    // See the StdException scenario above re: the " (at file:line)" suffix
    // now appended to lastExceptionMessage.
    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Postcondition" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  NonStdException: exceptionCount=" << recorder.exceptionCount << " step=\""
                  << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                  << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 5: TestContext::Set/Get round-trip with a real type
// (shared_ptr<SmallStruct>), including that mutations through the shared
// pointer are visible on a later Get of the same key.
// ---------------------------------------------------------------------

struct SmallStruct {
    int id;
    std::string label;
};

void SetupStructContext(TestContext& context) {
    context.Set("thing", std::make_shared<SmallStruct>(SmallStruct{7, "seven"}));
}

bool StepPreconditionStructRoundTrips(TestContext& context) {
    auto thing = context.Get<std::shared_ptr<SmallStruct>>("thing");
    return thing->id == 7 && thing->label == "seven";
}

bool StepActionMutatesStruct(TestContext& context) {
    auto thing = context.Get<std::shared_ptr<SmallStruct>>("thing");
    thing->id = 99;
    return true;
}

bool StepThenMutationIsVisible(TestContext& context) {
    auto thing = context.Get<std::shared_ptr<SmallStruct>>("thing");
    return thing->id == 99;
}

bool RunContextRoundTripScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupStructContext);
        recorder.Wire(test);
        test.With(StepPreconditionStructRoundTrips)
            .When(StepActionMutatesStruct)
            .Then(StepThenMutationIsVisible);
    }

    const bool asExpected = recorder.conditionNotVerifiedCount == 0 && recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  ContextRoundTrip: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                  << " exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 6: TestContext::Get for a missing key throws std::out_of_range
// directly (outside of any BabyBehaveTest step), and separately, a step
// that triggers the same missing-key throw is safely funneled through
// BabyBehaveTest's onException callback.
// ---------------------------------------------------------------------

bool RunMissingKeyDirectThrowScenario() {
    TestContext context;
    bool threwOutOfRange = false;
    try {
        (void)context.Get<int>("does_not_exist");
    } catch (const std::out_of_range& e) {
        threwOutOfRange = (std::string(e.what()) == "Key not found: does_not_exist");
    } catch (...) {
        threwOutOfRange = false;
    }

    if (!threwOutOfRange) {
        std::cerr << "  MissingKeyDirectThrow: expected std::out_of_range with the exact message\n";
    }
    return threwOutOfRange;
}

bool StepPreconditionGetsMissingKey(TestContext& context) {
    (void)context.Get<int>("does_not_exist");
    return true; // never reached; Get() throws first
}

bool RunMissingKeyInStepScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionGetsMissingKey)
            .When(StepActionTrue)
            .Then(StepAndTrue);
    }

    // See the StdException scenario above re: the " (at file:line)" suffix
    // now appended to lastExceptionMessage.
    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Precondition" &&
                             recorder.lastExceptionMessage.starts_with("Key not found: does_not_exist") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  MissingKeyInStep: exceptionCount=" << recorder.exceptionCount << " step=\""
                  << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                  << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 7: the context-setup function itself throws. Per bdd.hpp's
// current behavior, this is caught in Execute() and reported through the
// condition-not-verified path (NOT the exception callback), and steps
// still run afterward without crashing the process.
// ---------------------------------------------------------------------

void SetupThatThrows(TestContext&) {
    throw std::runtime_error("context setup exploded");
}

bool RunContextSetupThrowsScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupThatThrows);
        recorder.Wire(test);
        test.With(StepPreconditionTrue).When(StepActionTrue).Then(StepAndTrue);
    }

    // lastConditionMessage now carries a " (at file:line)" suffix pointing
    // at the GivenA(SetupThatThrows) call a few lines above (when
    // __cpp_lib_source_location is available) - context-setup failures are
    // attributed to the Given/GivenA call site, since there is no AddStep
    // call for context setup itself.
    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with(
                                 "Exception caught in Context Setup: context setup exploded") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  ContextSetupThrows: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                  << " lastConditionMessage=\"" << recorder.lastConditionMessage
                  << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 8: SetCollectFailuresMode(true) - a 4-step chain where the
// Precondition and the And step deliberately fail (one via a false
// return, one via a thrown exception), while the Action and Then steps
// pass. Unlike every scenario above, this one does NOT install its own
// SetOnConditionNotVerifiedCallback/SetOnExceptionCallback: collect-
// failures mode is the mechanism under test here, so it must, on its
// own, keep the process from exiting AND run every step (proving a
// failed Precondition does not stop the And/Then that follow) AND report
// exactly which steps failed and why via the TestResult.
//
// Because BabyBehaveTest::Execute() normally only runs from the
// destructor (by which point the object - and any TestResult it could
// hand back - is gone), this scenario binds the test to a named
// variable and calls Execute() manually to get the TestResult while the
// object is still alive. The subsequent destructor call is then a no-op
// (idempotent, guarded by Execute()'s internal "already ran" flag).
// ---------------------------------------------------------------------

bool StepPreconditionFalseForCollect(TestContext&) { return false; }
bool StepActionTrueForCollect(TestContext&) { return true; }
bool StepAndThrowsForCollect(TestContext&) {
    throw std::runtime_error("and step exploded");
}
bool StepThenTrueForCollect(TestContext&) { return true; }

bool RunCollectFailuresModeScenario() {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionFalseForCollect)
        .When(StepActionTrueForCollect)
        .And(StepAndThrowsForCollect)
        .Then(StepThenTrueForCollect);

    const TestResult& result = test.Execute();
    g_collectFailuresResults.push_back(result);

    bool asExpected = !result.allPassed && result.steps.size() == 4;

    if (asExpected) {
        const auto& precondition = result.steps[0];
        const auto& action = result.steps[1];
        const auto& andStep = result.steps[2];
        const auto& then = result.steps[3];

        asExpected = precondition.stepLabel == "Precondition" &&
                     precondition.stepName == "StepPreconditionFalseForCollect" && !precondition.passed &&
                     precondition.message == "Precondition failed" &&

                     action.stepLabel == "Action" && action.stepName == "StepActionTrueForCollect" &&
                     action.passed && action.message.empty() &&

                     andStep.stepLabel == "And" && andStep.stepName == "StepAndThrowsForCollect" &&
                     !andStep.passed && andStep.message == "and step exploded" &&

                     then.stepLabel == "Postcondition" && then.stepName == "StepThenTrueForCollect" &&
                     then.passed && then.message.empty();

#if defined(__cpp_lib_source_location)
        // On toolchains with <source_location> support, every recorded
        // StepResult also carries the "file:line" of the With/When/And/
        // Then(...) call that registered it (see the AddStep() call sites
        // a few lines above), regardless of whether that step passed or
        // failed - this is what makes it possible to jump straight to the
        // failing step in a large test file instead of only knowing its
        // stringified function name.
        if (asExpected) {
            asExpected = !precondition.location.empty() && precondition.location.find("SelfTest.cpp") != std::string::npos &&
                         !action.location.empty() && action.location.find("SelfTest.cpp") != std::string::npos &&
                         !andStep.location.empty() && andStep.location.find("SelfTest.cpp") != std::string::npos &&
                         !then.location.empty() && then.location.find("SelfTest.cpp") != std::string::npos;
        }
#endif
    }

    if (!asExpected) {
        std::cerr << "  CollectFailuresMode: allPassed=" << result.allPassed << " steps.size()=" << result.steps.size()
                  << '\n';
        for (const auto& step : result.steps) {
            std::cerr << "    " << step.stepLabel << " \"" << step.stepName << "\" passed=" << step.passed
                      << " message=\"" << step.message << "\" location=\"" << step.location << "\"\n";
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 9: SoftCheck - an Action step records 3 named sub-checks (2
// passing, 1 failing) and returns their AND as its single overall bool.
// Run under SetCollectFailuresMode(true), the same pattern as Scenario 8:
// bind the test to a named variable and Execute() it manually to get the
// TestResult while the object is still alive. Confirms StepResult::message
// names exactly the failing sub-check (with its message) and mentions
// neither passing one.
// ---------------------------------------------------------------------

bool StepActionWithSoftChecks(TestContext& context) {
    SoftCheck checks(context);
    const int count = 15;
    checks.Check("has valid id", true);
    checks.Check("name matches", true);
    checks.Check("count in range", count >= 1 && count <= 10, "count was " + std::to_string(count));
    return checks.AllPassed();
}

bool RunSoftCheckCollectFailuresScenario() {
    BabyBehaveTest test = GivenA(SetupTrivialContext);
    test.SetCollectFailuresMode(true);
    test.With(StepPreconditionTrue)
        .When(StepActionWithSoftChecks)
        .Then(StepThenTrueForCollect /* reused as a trivially-true postcondition */);

    const TestResult& result = test.Execute();
    g_collectFailuresResults.push_back(result);

    bool asExpected = !result.allPassed && result.steps.size() == 3;

    if (asExpected) {
        const auto& precondition = result.steps[0];
        const auto& action = result.steps[1];
        const auto& then = result.steps[2];

        asExpected = precondition.passed && precondition.message.empty() &&

                     action.stepLabel == "Action" && action.stepName == "StepActionWithSoftChecks" &&
                     !action.passed && action.message == "Action failed: count in range (count was 15)" &&

                     then.passed && then.message.empty();
    }

    if (!asExpected) {
        std::cerr << "  SoftCheckCollectFailures: allPassed=" << result.allPassed << " steps.size()=" << result.steps.size()
                  << '\n';
        for (const auto& step : result.steps) {
            std::cerr << "    " << step.stepLabel << " \"" << step.stepName << "\" passed=" << step.passed
                      << " message=\"" << step.message << "\"\n";
        }
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 10: SoftCheck via the DEFAULT (non-opt-in-to-collect-failures)
// callback path. Reuses StepActionWithSoftChecks from Scenario 9 above -
// same recorder, same one-failing-two-passing sub-checks - but this time
// the test installs its own capture-instead-of-exit callbacks (the
// established CallbackRecorder pattern used by every other scenario in
// this file) rather than SetCollectFailuresMode(true), to prove the
// aggregate failure message also reaches the default
// onConditionNotVerified callback path, not just StepResult::message.
// ---------------------------------------------------------------------

bool RunSoftCheckDefaultCallbackScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionWithSoftChecks)
            .Then(StepAndTrue);
    }

    // See the FailingCondition scenario earlier re: the " (at file:line)"
    // suffix appended to lastConditionMessage.
    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with(
                                 "Action failed: count in range (count was 15)") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  SoftCheckDefaultCallback: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                  << " lastConditionMessage=\"" << recorder.lastConditionMessage
                  << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

} // namespace

int main() {
    int passCount = 0;
    int totalCount = 0;

    ReportScenario("HappyPath: full With/When/Then/And/Or/But chain succeeds", RunHappyPathScenario(), passCount,
                    totalCount);
    ReportScenario("FailingAction: Action returns false", RunFailingConditionScenario(), passCount, totalCount);
    ReportScenario("FailingPrecondition: Precondition returns false", RunFailingPreconditionScenario(), passCount,
                    totalCount);
    ReportScenario("StdException: step throws std::exception", RunStdExceptionScenario(), passCount, totalCount);
    ReportScenario("NonStdException: step throws a non-std::exception value", RunNonStdExceptionScenario(),
                    passCount, totalCount);
    ReportScenario("ContextRoundTrip: Set/Get shared_ptr<SmallStruct> round-trips and shares state",
                    RunContextRoundTripScenario(), passCount, totalCount);
    ReportScenario("MissingKeyDirectThrow: TestContext::Get on missing key throws std::out_of_range",
                    RunMissingKeyDirectThrowScenario(), passCount, totalCount);
    ReportScenario("MissingKeyInStep: missing-key throw inside a step is funneled through onException",
                    RunMissingKeyInStepScenario(), passCount, totalCount);
    ReportScenario("ContextSetupThrows: context setup exception is reported via onConditionNotVerified",
                    RunContextSetupThrowsScenario(), passCount, totalCount);
    ReportScenario("CollectFailuresMode: failures are recorded and execution continues past them",
                    RunCollectFailuresModeScenario(), passCount, totalCount);
    ReportScenario("SoftCheckCollectFailures: named sub-checks surface in StepResult::message",
                    RunSoftCheckCollectFailuresScenario(), passCount, totalCount);
    ReportScenario("SoftCheckDefaultCallback: named sub-checks surface in the default failure callback",
                    RunSoftCheckDefaultCallbackScenario(), passCount, totalCount);

    std::cout << '\n' << passCount << "/" << totalCount << " scenarios behaved as expected\n";

    // ---------------------------------------------------------------------
    // Reporters demo: g_collectFailuresResults now holds the TestResult from
    // both SetCollectFailuresMode(true) scenarios above (Scenario 8:
    // CollectFailuresMode, and Scenario 9: SoftCheckCollectFailures) - each
    // with a genuine mix of passing AND failing steps. Feed the combined
    // vector into BabyBehave::BDD::Reporters::ToJUnitXml()/ToTap() (see
    // include/BabyBehave/reporters.hpp) to produce one CI-consumable report
    // covering both scenarios' steps, print it, and also write it to disk so
    // it can be picked up as a CI artifact (e.g. GitHub Actions'
    // dorny/test-reporter, GitLab's "junit" artifact report type, or piped
    // straight into `prove`).
    const std::string junitXml = BabyBehave::BDD::Reporters::ToJUnitXml(g_collectFailuresResults, "BabyBehave.SelfTest");
    const std::string tap = BabyBehave::BDD::Reporters::ToTap(g_collectFailuresResults);

    std::cout << "\n--- JUnit XML report (selftest-results.xml) ---\n" << junitXml;
    std::cout << "\n--- TAP report (selftest-results.tap) ---\n" << tap;

    if (std::ofstream xmlFile("selftest-results.xml"); xmlFile) {
        xmlFile << junitXml;
    } else {
        std::cerr << "SelfTest: failed to write selftest-results.xml\n";
    }
    if (std::ofstream tapFile("selftest-results.tap"); tapFile) {
        tapFile << tap;
    } else {
        std::cerr << "SelfTest: failed to write selftest-results.tap\n";
    }

    return (passCount == totalCount) ? EXIT_SUCCESS : EXIT_FAILURE;
}
