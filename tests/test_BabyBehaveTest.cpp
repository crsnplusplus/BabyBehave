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

// ctest silences this binary's narration by default (BABYBEHAVE_QUIET=1,
// see tests/CMakeLists.txt) so it doesn't interleave with gtest's own
// [ RUN ]/[ OK ] output - which means every OTHER test in this UT suite
// exercises PrintLine's/PrintErrorLine's early-return branch, never their
// "actually write" branch. This test exercises the opposite: flips
// SetNarrationEnabled(true) to hit that branch, verifies real output
// appears, then flips back to false (matching this binary's default)
// so it doesn't leak narration into whatever test gtest runs next. Also
// pins NarrationStyle::Plain explicitly (rather than relying on it being
// the ambient default): this test's assertions check for Plain's literal
// "Given a:" text, which only NarrationStyleFlag()'s in-process default
// guarantees - a caller with BABYBEHAVE_STYLE=arrow/tree set in their own
// shell before invoking ctest would otherwise make this test spuriously
// fail, since Arrow/Tree never print that substring at all.
TEST(BabyBehaveTest, NarrationEnabledTrue_ProducesStdoutAndStderrOutput) {
    BabyBehave::BDD::SetNarrationEnabled(true);
    BabyBehave::BDD::SetNarrationStyle(BabyBehave::BDD::NarrationStyle::Plain);
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();

    {
        auto test = BabyBehave::BDD::GivenA([](BabyBehave::BDD::TestContext&) {});
        test.With([](BabyBehave::BDD::TestContext&) { return true; });
    }
    BabyBehave::BDD::TestContext context;
    try {
        (void)context.Get<int>("narrationToggleMissingKey");
    } catch (...) {
    }

    const std::string capturedOut = testing::internal::GetCapturedStdout();
    const std::string capturedErr = testing::internal::GetCapturedStderr();
    BabyBehave::BDD::SetNarrationEnabled(false);

    EXPECT_NE(capturedOut.find("Given a:"), std::string::npos);
    EXPECT_NE(capturedErr.find("Key not found:"), std::string::npos);
}

// Exercises BABYBEHAVE_STYLE's value-mapping (detail::ParseNarrationStyleEnv
// - see bdd.hpp's comment for why it's a pure function rather than
// per-value environment/process setup) plus the Arrow/Tree renderers
// themselves, which - like NarrationEnabledTrue_ProducesStdoutAndStderrOutput
// above - are otherwise dead code in this binary: ctest silences narration
// entirely here, and even with it enabled every other test in this suite
// uses the default Plain style. Runs all six step kinds (With/When/Then/
// And/Or/But) under both Arrow and Tree so ArrowKindWord()/TreeKindLabel()'s
// switches are each fully hit, plus one Tree scenario with And/Or/But but no
// Then, to reach RenderTree()'s "orphaned assertions" fallback branch.
TEST(BabyBehaveTest, NarrationStyle_ParseEnvAndRenderersProduceOutput) {
    EXPECT_EQ(BabyBehave::BDD::detail::ParseNarrationStyleEnv(nullptr), BabyBehave::BDD::NarrationStyle::Plain);
    EXPECT_EQ(BabyBehave::BDD::detail::ParseNarrationStyleEnv("arrow"), BabyBehave::BDD::NarrationStyle::Arrow);
    EXPECT_EQ(BabyBehave::BDD::detail::ParseNarrationStyleEnv("tree"), BabyBehave::BDD::NarrationStyle::Tree);
    EXPECT_EQ(BabyBehave::BDD::detail::ParseNarrationStyleEnv("bogus"), BabyBehave::BDD::NarrationStyle::Plain);

    BabyBehave::BDD::SetNarrationEnabled(true);
    testing::internal::CaptureStdout();

    BabyBehave::BDD::SetNarrationStyle(BabyBehave::BDD::NarrationStyle::Arrow);
    {
        auto test = BabyBehave::BDD::GivenA([](BabyBehave::BDD::TestContext&) {});
        test.With([](BabyBehave::BDD::TestContext&) { return true; })
            .When([](BabyBehave::BDD::TestContext&) { return true; })
            .Then([](BabyBehave::BDD::TestContext&) { return true; })
            .And([](BabyBehave::BDD::TestContext&) { return true; })
            .Or([](BabyBehave::BDD::TestContext&) { return true; })
            .But([](BabyBehave::BDD::TestContext&) { return true; });
    }

    BabyBehave::BDD::SetNarrationStyle(BabyBehave::BDD::NarrationStyle::Tree);
    {
        auto test = BabyBehave::BDD::GivenA([](BabyBehave::BDD::TestContext&) {});
        test.With([](BabyBehave::BDD::TestContext&) { return true; })
            .When([](BabyBehave::BDD::TestContext&) { return true; })
            .Then([](BabyBehave::BDD::TestContext&) { return true; })
            .And([](BabyBehave::BDD::TestContext&) { return true; })
            .Or([](BabyBehave::BDD::TestContext&) { return true; })
            .But([](BabyBehave::BDD::TestContext&) { return true; });
    }
    {
        auto test = BabyBehave::BDD::GivenA([](BabyBehave::BDD::TestContext&) {});
        test.SetCollectFailuresMode(true);
        test.With([](BabyBehave::BDD::TestContext&) { return true; })
            .And([](BabyBehave::BDD::TestContext&) { return true; })
            .Or([](BabyBehave::BDD::TestContext&) { return false; })
            .But([](BabyBehave::BDD::TestContext&) { return true; });
    }

    const std::string capturedOut = testing::internal::GetCapturedStdout();
    BabyBehave::BDD::SetNarrationStyle(BabyBehave::BDD::NarrationStyle::Plain);
    BabyBehave::BDD::SetNarrationEnabled(false);

    EXPECT_NE(capturedOut.find("-> Given a:"), std::string::npos);
    EXPECT_NE(capturedOut.find("GIVEN"), std::string::npos);
}

TEST(BabyBehaveTest, ConstructorLocationDefaultsToCallerWhenOmitted) {
    // Exercises the (testName, contextSetupFn, suppressGivenNarration) constructor
    // directly, letting `loc` default to std::source_location::current() instead of
    // being passed explicitly - RunFeature()'s own per-scenario construction now passes
    // loc explicitly, so this is this suite's only remaining caller of the default.
    BabyBehave::BDD::BabyBehaveTest test("ConstructorLocationDefault", [](BabyBehave::BDD::TestContext&) {}, true);
    test.SetCollectFailuresMode(true);
    test.With([](BabyBehave::BDD::TestContext&) { return true; });
    EXPECT_TRUE(test.Execute().allPassed);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
