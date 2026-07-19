// SelfTest_DefaultExceptionExit.cpp
//
// Sibling probe to test_SelfTest_DefaultConditionExit.cpp (see that file's
// header comment for the overall rationale): installs NO custom
// callbacks, so a step throwing a std::exception reaches
// BabyBehaveTest's genuine default onException callback (bdd.hpp's
// InitDefaultCallbacks(), ~lines 405-409), which prints to std::cerr and
// calls std::exit(EXIT_FAILURE).
//
// This process is EXPECTED to exit with a non-zero status; it is
// registered in tests/bdd/CMakeLists.txt with WILL_FAIL TRUE so ctest
// treats that as a pass.

#include <BabyBehave/bdd.hpp>

#include <stdexcept>

using namespace BabyBehave::BDD;

namespace {
void SetupTrivialContext(TestContext&) {}
bool StepThrows(TestContext&) { throw std::runtime_error("default exception exit probe"); }
} // namespace

int main() {
    // No SetOnConditionNotVerifiedCallback/SetOnExceptionCallback here on
    // purpose. The destructor runs Execute(), the step throws, and the
    // default onException callback exits the process.
    GivenA(SetupTrivialContext).With(StepThrows);

    // Unreachable: the line above exits the process before this point.
    return EXIT_SUCCESS;
}
