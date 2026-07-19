// SelfTest_ExceptionCoverage.cpp
//
// A second, independent "dogfood" translation unit alongside test_SelfTest.cpp
// (see that file's header comment for the overall rationale of the BBH
// self-hosted coverage suite). This file exists purely to exercise a batch
// of catch(...) / catch(const std::exception&) fallback paths inside
// include/BabyBehave/bdd.hpp's BabyBehaveTest::executeStep()/VerifyCondition()/
// SafeInvokeExceptionCallback() that test_SelfTest.cpp does not reach:
//
//   1. Context setup throwing a NON-std::exception type.
//   2. A Precondition (.With) step throwing a non-std::exception type.
//   3. An Action (.When) step throwing a non-std::exception type.
//   4. An And (.And) step throwing a non-std::exception type.
//   5. An Or (.Or) step throwing BOTH a std::exception and a non-std one.
//   6. A But (.But) step throwing BOTH a std::exception and a non-std one.
//   7. A Postcondition (.Then) step throwing a std::exception.
//   8. A custom SetOnConditionNotVerifiedCallback that itself throws.
//   9. A custom SetOnExceptionCallback that itself throws.
//  10. TestContext::Get<T> on a missing key, for a fresh T (std::string)
//      not already instantiated by test_SelfTest.cpp's RunMissingKeyDirectThrowScenario
//      (which uses int).
//
// Same structural pattern as test_SelfTest.cpp: a small CallbackRecorder
// helper installs capture-instead-of-exit callbacks (BabyBehaveTest's
// defaults call std::exit() on any failure/exception, which would kill the
// process before later scenarios - several of which are *expected* to
// fail/throw on purpose - ever ran), one RunXxxScenario() function per
// scenario returning bool, and a main() that runs every scenario and
// returns EXIT_SUCCESS/FAILURE based on the pass count. This is a
// separate, self-contained TU (not #including the other .cpp file), so
// the scenario functions themselves are duplicated locally on purpose -
// but the ReportScenario()/mismatch-diagnostic machinery is shared via
// SelfTestDiagnostics.hpp (see that header's comment for why: this file
// used to keep its own copy, which fell behind test_SelfTest.cpp's).

#include <BabyBehave/bdd.hpp>

#include "SelfTestDiagnostics.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;

namespace {

// Captures what the library reported instead of letting the default
// callbacks terminate the process. Same shape as test_SelfTest.cpp's
// CallbackRecorder.
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

void SetupTrivialContext(TestContext&) {}

bool StepPreconditionTrue(TestContext&) { return true; }
bool StepActionTrue(TestContext&) { return true; }
bool StepPostconditionTrue(TestContext&) { return true; }
bool StepAndTrue(TestContext&) { return true; }
bool StepOrTrue(TestContext&) { return true; }
bool StepButTrue(TestContext&) { return true; }

// ---------------------------------------------------------------------
// Scenario 1: context setup throws a NON-std::exception type - exercises
// the catch(...) block in Execute() (bdd.hpp ~373-375), sibling to
// test_SelfTest.cpp's RunContextSetupThrowsScenario (which uses a
// std::exception). Reported via onConditionNotVerified with the generic
// "unknown non-std::exception type thrown" message, never onException.
// ---------------------------------------------------------------------

void SetupThatThrowsNonStdType(TestContext&) {
    throw 42; // deliberately not a std::exception
}

bool RunContextSetupThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupThatThrowsNonStdType);
        recorder.Wire(test);
        test.With(StepPreconditionTrue).When(StepActionTrue).Then(StepPostconditionTrue);
    }

    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with(
                                 "Exception caught in Context Setup: unknown non-std::exception type thrown") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  ContextSetupThrowsNonStd: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                   << " lastConditionMessage=\"" << recorder.lastConditionMessage
                   << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 2: a Precondition (.With) step throws a non-std::exception type
// - exercises executeStep<Precondition>'s catch(...) block (bdd.hpp
// ~430-433).
// ---------------------------------------------------------------------

bool StepPreconditionThrowsNonStdType(TestContext&) {
    throw std::string("precondition blew up");
}

bool RunPreconditionThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionThrowsNonStdType).When(StepActionTrue).Then(StepPostconditionTrue);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Precondition" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  PreconditionThrowsNonStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 3: an Action (.When) step throws a non-std::exception type -
// exercises executeStep<Action>'s catch(...) block (bdd.hpp ~442-445).
// ---------------------------------------------------------------------

bool StepActionThrowsNonStdType(TestContext&) {
    throw 7;
}

bool RunActionThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue).When(StepActionThrowsNonStdType).Then(StepPostconditionTrue);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Action" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  ActionThrowsNonStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 4: an And (.And) step throws a non-std::exception type -
// exercises executeStep<And>'s catch(...) block (bdd.hpp ~466-469).
// ---------------------------------------------------------------------

bool StepAndThrowsNonStdType(TestContext&) {
    throw 3.14;
}

bool RunAndThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionTrue)
            .And(StepAndThrowsNonStdType);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "And condition" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  AndThrowsNonStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 5a/5b: an Or (.Or) step throws - once with a std::exception
// (executeStep<Or>'s catch(const std::exception&), bdd.hpp ~475-476) and
// once with a non-std::exception type (its catch(...), bdd.hpp ~478-481).
// Run as two separate scenarios so both branches are independently
// verified.
// ---------------------------------------------------------------------

bool StepOrThrowsStdException(TestContext&) {
    throw std::runtime_error("or step std exploded");
}

bool RunOrThrowsStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionTrue)
            .Or(StepOrThrowsStdException);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Or condition" &&
                             recorder.lastExceptionMessage.starts_with("or step std exploded") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  OrThrowsStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

bool StepOrThrowsNonStdType(TestContext&) {
    throw 'x';
}

bool RunOrThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionTrue)
            .Or(StepOrThrowsNonStdType);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Or condition" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  OrThrowsNonStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 6a/6b: a But (.But) step throws - once with a std::exception
// (executeStep<But>'s catch(const std::exception&), bdd.hpp ~487-488) and
// once with a non-std::exception type (its catch(...), bdd.hpp ~490-493).
// ---------------------------------------------------------------------

bool StepButThrowsStdException(TestContext&) {
    throw std::logic_error("but step std exploded");
}

bool RunButThrowsStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionTrue)
            .But(StepButThrowsStdException);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "But condition" &&
                             recorder.lastExceptionMessage.starts_with("but step std exploded") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  ButThrowsStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

bool StepButThrowsNonStdType(TestContext&) {
    throw std::vector<int>{1, 2, 3};
}

bool RunButThrowsNonStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue)
            .When(StepActionTrue)
            .Then(StepPostconditionTrue)
            .But(StepButThrowsNonStdType);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "But condition" &&
                             recorder.lastExceptionMessage.starts_with("unknown non-std::exception type thrown") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  ButThrowsNonStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 7: a Postcondition (.Then) step throws a std::exception (as
// opposed to test_SelfTest.cpp's RunNonStdExceptionScenario, which has a
// Postcondition throw a non-std type) - exercises
// executeStep<Postcondition>'s catch(const std::exception&) block (bdd.hpp
// ~451-452).
// ---------------------------------------------------------------------

bool StepPostconditionThrowsStdException(TestContext&) {
    throw std::runtime_error("postcondition std exploded");
}

bool RunPostconditionThrowsStdScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue).When(StepActionTrue).Then(StepPostconditionThrowsStdException);
    }

    const bool asExpected = recorder.exceptionCount == 1 && recorder.lastExceptionStep == "Postcondition" &&
                             recorder.lastExceptionMessage.starts_with("postcondition std exploded") &&
                             recorder.conditionNotVerifiedCount == 0;
    if (!asExpected) {
        std::cerr << "  PostconditionThrowsStd: exceptionCount=" << recorder.exceptionCount << " step=\""
                   << recorder.lastExceptionStep << "\" message=\"" << recorder.lastExceptionMessage
                   << "\" conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 8: a custom SetOnConditionNotVerifiedCallback that itself
// throws - exercises the outer catch(...) in VerifyCondition() (bdd.hpp
// ~566-571, "ignoring to avoid std::terminate()"). The process must not
// crash; we just confirm the throwing callback ran (via a side-channel
// counter) and that Execute() completed normally afterward.
// ---------------------------------------------------------------------

int g_throwingConditionCallbackCount = 0;

bool RunThrowingConditionCallbackScenario() {
    g_throwingConditionCallbackCount = 0;
    bool completedWithoutCrashing = false;
    {
        auto test = GivenA(SetupTrivialContext);
        test.SetOnConditionNotVerifiedCallback([](const std::string&) {
            ++g_throwingConditionCallbackCount;
            throw std::runtime_error("condition callback itself misbehaves");
        });
        test.SetOnExceptionCallback([](const std::string&, const std::exception&) {
            // Not expected to fire in this scenario; left as a no-op safety
            // net so a future refactor can't silently reintroduce a call to
            // the (std::exit-ing) default here.
        });
        test.With(StepPreconditionTrue)
            .When([](TestContext&) { return false; }) // Action fails -> onConditionNotVerified fires -> throws.
            .Then(StepPostconditionTrue);
    } // If we get here, VerifyCondition's catch(...) safely swallowed it.
    completedWithoutCrashing = true;

    const bool asExpected = completedWithoutCrashing && g_throwingConditionCallbackCount == 1;
    if (!asExpected) {
        std::cerr << "  ThrowingConditionCallback: completedWithoutCrashing=" << completedWithoutCrashing
                   << " g_throwingConditionCallbackCount=" << g_throwingConditionCallbackCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 9: a custom SetOnExceptionCallback that itself throws -
// exercises SafeInvokeExceptionCallback's outer catch(...) (bdd.hpp
// ~598-601, same "ignoring to avoid std::terminate()" reasoning as
// scenario 8 above).
// ---------------------------------------------------------------------

int g_throwingExceptionCallbackCount = 0;

bool StepActionThrowsForCallbackScenario(TestContext&) {
    throw std::runtime_error("action step exploded, callback will misbehave too");
}

bool RunThrowingExceptionCallbackScenario() {
    g_throwingExceptionCallbackCount = 0;
    bool completedWithoutCrashing = false;
    {
        auto test = GivenA(SetupTrivialContext);
        test.SetOnConditionNotVerifiedCallback([](const std::string&) {
            // Not expected to fire in this scenario.
        });
        test.SetOnExceptionCallback([](const std::string&, const std::exception&) {
            ++g_throwingExceptionCallbackCount;
            throw std::logic_error("exception callback itself misbehaves");
        });
        test.With(StepPreconditionTrue).When(StepActionThrowsForCallbackScenario).Then(StepPostconditionTrue);
    } // If we get here, SafeInvokeExceptionCallback's catch(...) safely swallowed it.
    completedWithoutCrashing = true;

    const bool asExpected = completedWithoutCrashing && g_throwingExceptionCallbackCount == 1;
    if (!asExpected) {
        std::cerr << "  ThrowingExceptionCallback: completedWithoutCrashing=" << completedWithoutCrashing
                   << " g_throwingExceptionCallbackCount=" << g_throwingExceptionCallbackCount << '\n';
    }
    return asExpected;
}

// ---------------------------------------------------------------------
// Scenario 10: TestContext::Get<T> on a missing key, for T = std::string -
// a template instantiation distinct from test_SelfTest.cpp's
// RunMissingKeyDirectThrowScenario (which instantiates Get<int>), so this
// exercises a fresh not-found throw path for gcov's line-coverage purposes.
// ---------------------------------------------------------------------

bool RunMissingKeyDirectThrowStringScenario() {
    TestContext context;
    bool threwOutOfRange = false;
    try {
        (void)context.Get<std::string>("also_does_not_exist");
    } catch (const std::out_of_range& e) {
        threwOutOfRange = (std::string(e.what()) == "Key not found: also_does_not_exist");
    } catch (...) {
        threwOutOfRange = false;
    }

    if (!threwOutOfRange) {
        std::cerr << "  MissingKeyDirectThrowString: expected std::out_of_range with the exact message\n";
    }
    return threwOutOfRange;
}

// ---------------------------------------------------------------------
// Scenario 11: an Action step records TWO failing SoftCheck sub-checks
// (plus one passing one) - exercises the "; " join separator in
// FormatFailedSoftChecks() (bdd.hpp ~515), only reached when more than
// one failed sub-check needs joining. test_SelfTest.cpp's SoftCheck
// scenarios only ever have a single failing sub-check.
// ---------------------------------------------------------------------

bool StepActionSoftCheckTwoFailures(TestContext& context) {
    SoftCheck checks(context);
    checks.Check("has valid id", true);
    checks.Check("name matches", false, "expected Bob, got Alice");
    checks.Check("count in range", false, "count was 15");
    return checks.AllPassed();
}

bool RunSoftCheckTwoFailuresScenario() {
    CallbackRecorder recorder;
    {
        auto test = GivenA(SetupTrivialContext);
        recorder.Wire(test);
        test.With(StepPreconditionTrue).When(StepActionSoftCheckTwoFailures).Then(StepPostconditionTrue);
    }

    const bool asExpected = recorder.conditionNotVerifiedCount == 1 &&
                             recorder.lastConditionMessage.starts_with(
                                 "Action failed: name matches (expected Bob, got Alice); count in range (count was 15)") &&
                             recorder.exceptionCount == 0;
    if (!asExpected) {
        std::cerr << "  SoftCheckTwoFailures: conditionNotVerifiedCount=" << recorder.conditionNotVerifiedCount
                   << " lastConditionMessage=\"" << recorder.lastConditionMessage
                   << "\" exceptionCount=" << recorder.exceptionCount << '\n';
    }
    return asExpected;
}

} // namespace

int main(int argc, char** argv) {
    ParseCommandLine(argc, argv);

    int passCount = 0;
    int totalCount = 0;

    // Every scenario below drives a BabyBehaveTest chain whose whole point
    // is that some step/callback misbehaves - so BabyBehave's own narration
    // always reports FAIL for these, hence expectedLabel="FAIL" throughout.
    // MissingKeyDirectThrowString is the one exception: it never constructs
    // a BabyBehaveTest at all (just a direct TestContext::Get<T> call), so
    // there's no narration to capture - it keeps the terse bool overload,
    // same treatment as test_SelfTest.cpp's sibling MissingKeyDirectThrow.
    ReportScenario("ContextSetupThrowsNonStd: context setup throws a non-std::exception type", "FAIL",
                    [] { return RunContextSetupThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("PreconditionThrowsNonStd: With step throws a non-std::exception type", "FAIL",
                    [] { return RunPreconditionThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("ActionThrowsNonStd: When step throws a non-std::exception type", "FAIL",
                    [] { return RunActionThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("AndThrowsNonStd: And step throws a non-std::exception type", "FAIL",
                    [] { return RunAndThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("OrThrowsStd: Or step throws a std::exception", "FAIL", [] { return RunOrThrowsStdScenario(); },
                    passCount, totalCount);
    ReportScenario("OrThrowsNonStd: Or step throws a non-std::exception type", "FAIL",
                    [] { return RunOrThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("ButThrowsStd: But step throws a std::exception", "FAIL", [] { return RunButThrowsStdScenario(); },
                    passCount, totalCount);
    ReportScenario("ButThrowsNonStd: But step throws a non-std::exception type", "FAIL",
                    [] { return RunButThrowsNonStdScenario(); }, passCount, totalCount);
    ReportScenario("PostconditionThrowsStd: Then step throws a std::exception", "FAIL",
                    [] { return RunPostconditionThrowsStdScenario(); }, passCount, totalCount);
    ReportScenario("ThrowingConditionCallback: SetOnConditionNotVerifiedCallback itself throws", "FAIL",
                    [] { return RunThrowingConditionCallbackScenario(); }, passCount, totalCount);
    ReportScenario("ThrowingExceptionCallback: SetOnExceptionCallback itself throws", "FAIL",
                    [] { return RunThrowingExceptionCallbackScenario(); }, passCount, totalCount);
    ReportScenario("MissingKeyDirectThrowString: TestContext::Get<std::string> on missing key throws std::out_of_range",
                    RunMissingKeyDirectThrowStringScenario(), passCount, totalCount);
    ReportScenario("SoftCheckTwoFailures: two failed sub-checks are joined with \"; \"", "FAIL",
                    [] { return RunSoftCheckTwoFailuresScenario(); }, passCount, totalCount);

    std::cout << '\n' << passCount << "/" << totalCount << " scenarios behaved as expected\n";

    return (passCount == totalCount) ? EXIT_SUCCESS : EXIT_FAILURE;
}
