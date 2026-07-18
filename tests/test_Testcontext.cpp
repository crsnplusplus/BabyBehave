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
        (void)context.Get<int>(key);
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

    ASSERT_THROW((void)context.Get<std::string>(key), std::bad_any_cast);
}

TEST(TestContext, testGetNonExistentKeyCorrectType) {
    BabyBehave::BDD::TestContext context;
    std::string key = "nonExistentKey";

    ASSERT_THROW((void)context.Get<int>(key), std::out_of_range);
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

TEST(TestContext, testTypedContextKeyRoundTrips) {
    BabyBehave::BDD::TestContext context;

    static constexpr BabyBehave::BDD::TestContext::ContextKey<int> kIntKey{"typedIntKey"};
    static constexpr BabyBehave::BDD::TestContext::ContextKey<std::string> kStringKey{"typedStringKey"};

    context.Set(kIntKey, 42);
    context.Set(kStringKey, std::string("hello"));

    int retrievedInt = context.Get(kIntKey);
    std::string retrievedString = context.Get(kStringKey);

    ASSERT_EQ(42, retrievedInt);
    ASSERT_EQ("hello", retrievedString);

    // A ContextKey<T> only accepts values/reads of type T at compile time, so a typo'd
    // key name or a wrong-type Get/Set call site is caught by the compiler rather than
    // throwing std::bad_any_cast or std::out_of_range at runtime. The following would
    // fail to compile because kIntKey is a ContextKey<int>:
    //   std::string wrongType = context.Get(kIntKey);       // no viable conversion
    //   context.Set(kIntKey, std::string("not an int"));    // no matching Set overload

    // The typed-key API is purely additive: the underlying storage is still the same
    // string-keyed map used by the plain Set<T>/Get<T> overloads, so an existing
    // string-based call site can read back a value written through its typed key.
    int retrievedViaStringKey = context.Get<int>("typedIntKey");
    ASSERT_EQ(42, retrievedViaStringKey);
}

// testNonExistentKey/testGetNonExistentKeyCorrectType above already drive
// the not-found throw path (bdd.hpp's TestContext::Get<T>) for T=int; each
// of the other T's used elsewhere in this file is a distinct template
// instantiation with its own copy of that throw path, so it needs its own
// missing-key call to be exercised for coverage purposes.
TEST(TestContext, testGetNonExistentKeyStringType) {
    BabyBehave::BDD::TestContext context;
    ASSERT_THROW((void)context.Get<std::string>("missingStringKey"), std::out_of_range);
}

TEST(TestContext, testGetNonExistentKeyVectorType) {
    BabyBehave::BDD::TestContext context;
    ASSERT_THROW((void)(context.Get<std::vector<int>>("missingVectorKey")), std::out_of_range);
}

TEST(TestContext, testGetNonExistentKeyMapType) {
    BabyBehave::BDD::TestContext context;
    ASSERT_THROW((void)(context.Get<std::map<std::string, int>>("missingMapKey")), std::out_of_range);
}

TEST(TestContext, testGetNonExistentKeyCustomClassType) {
    BabyBehave::BDD::TestContext context;
    ASSERT_THROW((void)context.Get<MyClass>("missingCustomClassKey"), std::out_of_range);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}