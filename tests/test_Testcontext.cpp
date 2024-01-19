#include <BabyBehave/bdd.hpp>
#include <gtest/gtest.h>

#include <vector>
#include <map>
#include <string>
#include <any>

TEST(TestContext, testSetMethodStoresValueCorrectly) {
    BabyBehave::BDD::TestContext context;
    std::string key = "testKey";
    int value = 123;

    context.Set(key, value);

    int retrievedValue = std::any_cast<int>(context.Get<int>(key));
    ASSERT_EQ(value, retrievedValue);
}

TEST(TestContext, testOverwriteReplacesExistingValue) {
    BabyBehave::BDD::TestContext context;
    std::string key = "testKey";
    int value1 = 123;
    int value2 = 456;

    context.Set(key, value1);
    context.Set(key, value2);

    int retrievedValue = std::any_cast<int>(context.Get<int>(key));
    ASSERT_EQ(value2, retrievedValue);
}

TEST(TestContext, testSetOverwritesExistingKey) {
    BabyBehave::BDD::TestContext context;
    std::string key = "existingKey";
    int value1 = 123;
    int value2 = 456;

    context.Set(key, value1);
    context.Set(key, value2);

    int retrievedValue = std::any_cast<int>(context.Get<int>(key));
    ASSERT_EQ(value2, retrievedValue);
}

TEST(TestContext, testSetDifferentTypes) {
    BabyBehave::BDD::TestContext context;
    std::string key1 = "key1";
    std::string key2 = "key2";
    int value1 = 123;
    std::string value2 = "abc";

    context.Set(key1, value1);
    context.Set(key2, value2);

    int retrievedValue1 = std::any_cast<int>(context.Get<int>(key1));
    std::string retrievedValue2 = std::any_cast<std::string>(context.Get<std::string>(key2));

    ASSERT_EQ(value1, retrievedValue1);
    ASSERT_EQ(value2, retrievedValue2);
}

TEST(TestContext, testNonExistentKey) {
    BabyBehave::BDD::TestContext context;
    std::string key = "nonExistentKey";
    
    try {
        context.Get<int>(key);
    } catch (const std::exception& e) {
        ASSERT_STREQ("Key not found: nonExistentKey", e.what());
        return;
    }
    FAIL() << "Expected std::out_of_range";
}

TEST(TestContext, testGetExistingKey) {
    BabyBehave::BDD::TestContext context;
    std::string key = "existingKey";
    int value = 123;

    context.Set(key, value);

    int retrievedValue = std::any_cast<int>(context.Get<int>(key));
    ASSERT_EQ(value, retrievedValue);
}

TEST(TestContext, testDifferentTypes) {
    BabyBehave::BDD::TestContext context;
    std::string key1 = "key1";
    int value1 = 123;
    std::string key2 = "key2";
    std::string value2 = "abc";

    context.Set(key1, value1);
    context.Set(key2, value2);

    int retrievedValue1 = std::any_cast<int>(context.Get<int>(key1));
    std::string retrievedValue2 = std::any_cast<std::string>(context.Get<std::string>(key2));

    ASSERT_EQ(value1, retrievedValue1);
    ASSERT_EQ(value2, retrievedValue2);
}

TEST(TestContext, testGetWrongType) {
    BabyBehave::BDD::TestContext context;
    std::string key = "existingKey";
    int value = 123;

    context.Set(key, value);

    ASSERT_THROW(context.Get<std::string>(key), std::bad_any_cast);
}

TEST(TestContext, testGetNonExistentKeyCorrectType) {
    BabyBehave::BDD::TestContext context;
    std::string key = "nonExistentKey";

    ASSERT_THROW(context.Get<int>(key), std::out_of_range);
}

TEST(TestContext, testSetGetComplexType) {
    BabyBehave::BDD::TestContext context;
    std::string key = "key";
    std::vector<int> value = {1, 2, 3};

    context.Set(key, value);

    std::vector<int> retrievedValue = std::any_cast<std::vector<int>>(context.Get<std::vector<int>>(key));

    ASSERT_EQ(value, retrievedValue);
}

TEST(TestContext, testSetGetVector) {
    BabyBehave::BDD::TestContext context;
    std::string key = "key";
    std::vector<int> value = {1, 2, 3};

    context.Set(key, value);

    std::vector<int> retrievedValue = std::any_cast<std::vector<int>>(context.Get<std::vector<int>>(key));

    ASSERT_EQ(value, retrievedValue);
}

TEST(TestContext, testSetGetMap) {
    BabyBehave::BDD::TestContext context;
    std::string key = "key";
    std::map<std::string, int> value = {{"a", 1}, {"b", 2}, {"c", 3}};

    context.Set(key, value);

    std::map<std::string, int> retrievedValue = std::any_cast<std::map<std::string, int>>(context.Get<std::map<std::string, int>>(key));

    ASSERT_EQ(value, retrievedValue);
}

class MyClass {
public:
    int a;
    std::string b;
};

TEST(TestContext, testSetGetCustomClass) {
    BabyBehave::BDD::TestContext context;
    std::string key = "key";
    MyClass value;
    value.a = 123;
    value.b = "abc";

    context.Set(key, value);

    MyClass retrievedValue = std::any_cast<MyClass>(context.Get<MyClass>(key));

    ASSERT_EQ(value.a, retrievedValue.a);
    ASSERT_EQ(value.b, retrievedValue.b);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}