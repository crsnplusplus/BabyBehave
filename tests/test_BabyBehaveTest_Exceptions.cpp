#include <gtest/gtest.h>
#include <BabyBehave/bdd.hpp>

#include <string>

using namespace BabyBehave::BDD;

// ---------------------------------------------------------------------
// Precondition (.With)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, PreconditionThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) -> bool {
        throw std::runtime_error("precondition boom");
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Precondition");
    EXPECT_TRUE(capturedWhat.starts_with("precondition boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, PreconditionThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) -> bool {
        throw 42;
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Precondition");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// Action (.When)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, ActionThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.When([](TestContext&) -> bool {
        throw std::runtime_error("action boom");
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Action");
    EXPECT_TRUE(capturedWhat.starts_with("action boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, ActionThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.When([](TestContext&) -> bool {
        throw std::string("boom");
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Action");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// Postcondition (.Then)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, PostconditionThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.Then([](TestContext&) -> bool {
        throw std::runtime_error("postcondition boom");
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Postcondition");
    EXPECT_TRUE(capturedWhat.starts_with("postcondition boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, PostconditionThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.Then([](TestContext&) -> bool {
        throw 7;
        return true;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Postcondition");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// And (.And)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, AndThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .And([](TestContext&) -> bool {
            throw std::runtime_error("and boom");
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "And condition");
    EXPECT_TRUE(capturedWhat.starts_with("and boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, AndThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .And([](TestContext&) -> bool {
            throw std::string("boom");
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "And condition");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// Or (.Or)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, OrThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .Or([](TestContext&) -> bool {
            throw std::runtime_error("or boom");
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Or condition");
    EXPECT_TRUE(capturedWhat.starts_with("or boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, OrThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .Or([](TestContext&) -> bool {
            throw 99;
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "Or condition");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// But (.But)
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, ButThrowsStdException) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .But([](TestContext&) -> bool {
            throw std::runtime_error("but boom");
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "But condition");
    EXPECT_TRUE(capturedWhat.starts_with("but boom")) << capturedWhat;
}

TEST(BabyBehaveTestExceptions, ButThrowsNonStdType) {
    int callCount = 0;
    std::string capturedStep;
    std::string capturedWhat;

    auto test = GivenA([](TestContext&) {});
    test.SetOnExceptionCallback([&](const std::string& step, const std::exception& e) {
        ++callCount;
        capturedStep = step;
        capturedWhat = e.what();
    });
    test.With([](TestContext&) { return true; })
        .But([](TestContext&) -> bool {
            throw std::string("boom");
            return true;
        });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedStep, "But condition");
    EXPECT_NE(capturedWhat.find("unknown non-std::exception type thrown"), std::string::npos) << capturedWhat;
}

// ---------------------------------------------------------------------
// Context setup (GivenA) exceptions
// ---------------------------------------------------------------------

TEST(BabyBehaveTestExceptions, ContextSetupThrowsStdException) {
    int callCount = 0;
    std::string capturedMsg;

    auto test = GivenA([](TestContext&) {
        throw std::runtime_error("setup boom");
    });
    test.SetOnConditionNotVerifiedCallback([&](const std::string& errorMsg) {
        ++callCount;
        capturedMsg = errorMsg;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_NE(capturedMsg.find("Exception caught in Context Setup: setup boom"), std::string::npos) << capturedMsg;
}

TEST(BabyBehaveTestExceptions, ContextSetupThrowsNonStdType) {
    int callCount = 0;
    std::string capturedMsg;

    auto test = GivenA([](TestContext&) -> void {
        throw 13;
    });
    test.SetOnConditionNotVerifiedCallback([&](const std::string& errorMsg) {
        ++callCount;
        capturedMsg = errorMsg;
    });
    test.Execute();

    EXPECT_EQ(callCount, 1);
    EXPECT_NE(capturedMsg.find("Exception caught in Context Setup: unknown non-std::exception type thrown"), std::string::npos) << capturedMsg;
}
