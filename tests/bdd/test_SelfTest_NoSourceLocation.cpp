// SelfTest_NoSourceLocation.cpp
//
// Every other file under tests/bdd/ (and under tests/) is compiled with
// <source_location> support (this toolchain defines
// __cpp_lib_source_location), so bdd.hpp's step/context-setup locations
// are always non-empty "file:line" strings -- which means
// SafeInvokeExceptionCallback's `if (location.empty())` branch (bdd.hpp
// ~592-593) is genuinely unreachable in every other translation unit:
// FormatLocation() always concatenates at least ":" + a line number, so
// the result can never be an empty string.
//
// That branch is NOT dead code in general -- it is exactly what runs on a
// toolchain without <source_location> support, where AddStep()/GivenA()
// push a plain empty std::string for each step's location (see bdd.hpp's
// #else branch, ~lines 328-335). This TU deliberately forces bdd.hpp down
// that #else path within itself (and ONLY itself) by undefining the
// standard feature-test macro right before including the header, so the
// location.empty()==true branch gets exercised for real, matching actual
// behavior on such a toolchain -- without touching bdd.hpp itself or
// affecting any other translation unit.
#include <version>
#undef __cpp_lib_source_location
#include <BabyBehave/bdd.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace BabyBehave::BDD;

namespace {

void SetupTrivialContext(TestContext&) {}
bool StepPreconditionTrue(TestContext&) { return true; }
bool StepThrows(TestContext&) { throw std::runtime_error("no-source-location probe"); }

bool RunExceptionWithEmptyLocationScenario() {
    int exceptionCount = 0;
    std::string lastMessage;
    {
        auto test = GivenA(SetupTrivialContext);
        test.SetOnConditionNotVerifiedCallback([](const std::string&) {});
        test.SetOnExceptionCallback([&](const std::string&, const std::exception& e) {
            ++exceptionCount;
            lastMessage = e.what();
        });
        test.With(StepPreconditionTrue).When(StepThrows);
    }

    // On a toolchain without <source_location> support, step locations
    // are always empty, so SafeInvokeExceptionCallback takes the
    // location.empty()==true branch: the callback receives the ORIGINAL
    // exception object unchanged, with no " (at file:line)" suffix
    // appended.
    const bool asExpected = exceptionCount == 1 && lastMessage == "no-source-location probe" &&
                             lastMessage.find(" (at ") == std::string::npos;
    if (!asExpected) {
        std::cerr << "  ExceptionWithEmptyLocation: exceptionCount=" << exceptionCount << " lastMessage=\""
                   << lastMessage << "\"\n";
    }
    return asExpected;
}

} // namespace

int main() {
    const bool ok = RunExceptionWithEmptyLocationScenario();
    std::cout << (ok ? "[OK]   " : "[FAIL] ")
              << "ExceptionWithEmptyLocation: step location.empty()==true branch (no <source_location> build)\n";
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
