// test_BabyBehaveTest_NoSourceLocation.cpp
//
// Every other UT file is compiled with <source_location> support (this
// toolchain defines __cpp_lib_source_location), so bdd.hpp's step
// locations are always non-empty "file:line" strings -- which means
// SafeInvokeExceptionCallback's `if (location.empty())` branch (bdd.hpp
// ~592-593) is unreachable in every other translation unit: FormatLocation()
// always concatenates at least ":" + a line number, so the result can
// never be an empty string.
//
// That branch is real behavior on a toolchain WITHOUT <source_location>
// support (bdd.hpp's #else path pushes a plain empty std::string for each
// step's location -- see bdd.hpp ~lines 328-335). This TU forces bdd.hpp
// down that #else path within itself (and ONLY itself) by undefining the
// standard feature-test macro right before including the header, so the
// location.empty()==true branch is exercised for real -- without touching
// bdd.hpp itself or affecting any other translation unit.
#include <gtest/gtest.h>

#include <version>
#undef __cpp_lib_source_location
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

namespace {
void SetupTrivialContext(TestContext&) {}
bool StepPreconditionTrue(TestContext&) { return true; }
bool StepThrows(TestContext&) { throw std::runtime_error("no-source-location probe"); }
} // namespace

TEST(BabyBehaveTestNoSourceLocation, ExceptionCallbackReceivesUnsuffixedMessageWhenLocationIsEmpty) {
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

    EXPECT_EQ(exceptionCount, 1);
    EXPECT_EQ(lastMessage, "no-source-location probe");
    EXPECT_EQ(lastMessage.find(" (at "), std::string::npos);
}
