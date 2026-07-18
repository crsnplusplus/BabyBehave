#include <gtest/gtest.h>
#include <BabyBehave/bdd.hpp>

#include <stdexcept>
#include <string>

using namespace BabyBehave::BDD;

// ---------------------------------------------------------------------
// A) Custom callback that itself throws - exercises the catch-all
// wrapping m_onConditionNotVerifiedCallback / m_onExceptionCallback in
// VerifyCondition() / SafeInvokeExceptionCallback() (bdd.hpp ~547-602).
// If that catch-all were missing/broken, the callback's own exception
// would propagate out of ~BabyBehaveTest() during stack unwinding (or
// out of Execute()) and terminate the process - so simply reaching the
// EXPECT_TRUE below, with the test process still alive, is proof the
// catch-all did its job.
// ---------------------------------------------------------------------

TEST(BabyBehaveTestCallbacks, OnConditionNotVerifiedCallbackThrows_DoesNotTerminate) {
    bool executionContinued = false;

    auto test = GivenA([](TestContext&) {});
    test.SetOnConditionNotVerifiedCallback([](const std::string&) {
        throw std::runtime_error("callback boom");
    });
    test.With([](TestContext&) -> bool {
        return false;
    });
    test.Execute();

    executionContinued = true;
    EXPECT_TRUE(executionContinued);
}

TEST(BabyBehaveTestCallbacks, OnExceptionCallbackThrows_DoesNotTerminate) {
    bool executionContinued = false;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([](const std::string&, const std::exception&) {
        throw std::runtime_error("callback boom");
    });
    test.When([](TestContext&) -> bool {
        throw std::runtime_error("step boom");
        return true;
    });
    test.Execute();

    executionContinued = true;
    EXPECT_TRUE(executionContinued);
}

// ---------------------------------------------------------------------
// B) Death tests for the DEFAULT (un-overridden) failure callbacks -
// InitDefaultCallbacks() (bdd.hpp ~398-409): no custom callback
// installed, so a failed condition / uncaught exception in a step
// prints to std::cerr and calls std::exit(EXIT_FAILURE).
// ---------------------------------------------------------------------

TEST(BabyBehaveTestDeathTests, DefaultOnConditionNotVerifiedCallback_ExitsWithFailure) {
    EXPECT_EXIT(
        {
            auto test = GivenA([](TestContext&) {});
            test.With([](TestContext&) -> bool {
                return false;
            });
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "");
}

TEST(BabyBehaveTestDeathTests, DefaultOnExceptionCallback_ExitsWithFailure) {
    EXPECT_EXIT(
        {
            auto test = GivenA([](TestContext&) {});
            test.When([](TestContext&) -> bool {
                throw std::runtime_error("step boom");
                return true;
            });
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "");
}
