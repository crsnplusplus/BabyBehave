#include <gtest/gtest.h>
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

TEST(BabyBehaveTest, ExecuteWithMultipleSteps) {
    BabyBehave::BDD::GivenA([](BabyBehave::BDD::TestContext& context) {
        context.Set("test_variable", 10);
    })
    .When([](BabyBehave::BDD::TestContext& context) {
        int value = context.Get<int>("test_variable");
        value += 5;
        context.Set("test_variable", value);
        return true;
    })
    .And([](BabyBehave::BDD::TestContext& context) {
        int value = context.Get<int>("test_variable");
        value *= 2;
        context.Set("test_variable", value);
        return true;
    })
    .Then([](BabyBehave::BDD::TestContext& context) {
        int value = context.Get<int>("test_variable");
        EXPECT_EQ(value, 30);
        return true;
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
