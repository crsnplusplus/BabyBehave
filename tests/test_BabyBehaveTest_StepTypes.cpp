#include <gtest/gtest.h>
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

// Test Precondition step that passes
TEST(BabyBehaveTest_StepTypes, PreconditionPasses) {
    GivenA([](TestContext& context) {
        context.Set("value", 5);
    })
    .With([](TestContext& context) {
        int val = context.Get<int>("value");
        return val > 0;
    })
    .Then([](TestContext& context) {
        int val = context.Get<int>("value");
        EXPECT_GT(val, 0);
        return true;
    });
}

// Test Precondition step that fails
TEST(BabyBehaveTest_StepTypes, PreconditionFails) {
    bool callback_invoked = false;
    std::string error_message;

    BabyBehaveTest test = GivenA([](TestContext& context) {
        context.Set("value", -5);
    });

    test.SetOnConditionNotVerifiedCallback([&](const std::string& msg) {
        callback_invoked = true;
        error_message = msg;
    });

    test.With([](TestContext& context) {
        int val = context.Get<int>("value");
        return val > 0;
    });

    test.Execute();

    EXPECT_TRUE(callback_invoked);
    EXPECT_TRUE(error_message.find("Precondition failed") != std::string::npos);
}

// Test Or step that passes
TEST(BabyBehaveTest_StepTypes, OrPasses) {
    GivenA([](TestContext& context) {
        context.Set("condition_met", true);
    })
    .When([](TestContext& context) {
        context.Set("counter", 1);
        return true;
    })
    .Or([](TestContext& context) {
        bool cond = context.Get<bool>("condition_met");
        return cond;
    })
    .Then([](TestContext& context) {
        int cnt = context.Get<int>("counter");
        EXPECT_EQ(cnt, 1);
        return true;
    });
}

// Test Or step that fails
TEST(BabyBehaveTest_StepTypes, OrFails) {
    bool callback_invoked = false;
    std::string error_message;

    BabyBehaveTest test = GivenA([](TestContext& context) {
        context.Set("condition_met", false);
    });

    test.SetOnConditionNotVerifiedCallback([&](const std::string& msg) {
        callback_invoked = true;
        error_message = msg;
    });

    test.When([](TestContext& context) {
        context.Set("counter", 1);
        return true;
    })
    .Or([](TestContext& context) {
        bool cond = context.Get<bool>("condition_met");
        return cond;
    });

    test.Execute();

    EXPECT_TRUE(callback_invoked);
    EXPECT_TRUE(error_message.find("Or condition failed") != std::string::npos);
}

// Test But step that passes
TEST(BabyBehaveTest_StepTypes, ButPasses) {
    GivenA([](TestContext& context) {
        context.Set("value", 10);
    })
    .When([](TestContext& context) {
        int val = context.Get<int>("value");
        val += 5;
        context.Set("value", val);
        return true;
    })
    .But([](TestContext& context) {
        int val = context.Get<int>("value");
        return val != 0;
    })
    .Then([](TestContext& context) {
        int val = context.Get<int>("value");
        EXPECT_EQ(val, 15);
        return true;
    });
}

// Test But step that fails
TEST(BabyBehaveTest_StepTypes, ButFails) {
    bool callback_invoked = false;
    std::string error_message;

    BabyBehaveTest test = GivenA([](TestContext& context) {
        context.Set("value", 10);
    });

    test.SetOnConditionNotVerifiedCallback([&](const std::string& msg) {
        callback_invoked = true;
        error_message = msg;
    });

    test.When([](TestContext& context) {
        context.Set("value", 0);
        return true;
    })
    .But([](TestContext& context) {
        int val = context.Get<int>("value");
        return val != 0;
    });

    test.Execute();

    EXPECT_TRUE(callback_invoked);
    EXPECT_TRUE(error_message.find("But condition failed") != std::string::npos);
}
