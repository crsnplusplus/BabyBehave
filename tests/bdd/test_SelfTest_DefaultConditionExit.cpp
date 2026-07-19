// SelfTest_DefaultConditionExit.cpp
//
// A single-purpose BBH probe: unlike every other file under tests/bdd/,
// this one installs NO custom callbacks at all, so BabyBehaveTest's
// genuine default onConditionNotVerified callback (bdd.hpp's
// InitDefaultCallbacks(), ~lines 401-404) fires for real -- it prints to
// std::cerr and calls std::exit(EXIT_FAILURE). That default path is
// otherwise unreachable in the BBH suite, since every other scenario file
// deliberately installs capture-instead-of-exit callbacks so later
// scenarios can still run afterward.
//
// This process is EXPECTED to exit with a non-zero status; it is
// registered in tests/bdd/CMakeLists.txt with WILL_FAIL TRUE so ctest
// treats that as a pass.

#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

namespace {
void SetupTrivialContext(TestContext&) {}
bool AlwaysFalse(TestContext&) { return false; }
} // namespace

int main() {
    // No SetOnConditionNotVerifiedCallback/SetOnExceptionCallback here on
    // purpose. The destructor runs Execute(), which calls VerifyCondition
    // with condition=false, which invokes the default callback -> exits.
    GivenA(SetupTrivialContext).With(AlwaysFalse);

    // Unreachable: the line above exits the process before this point.
    return EXIT_SUCCESS;
}
