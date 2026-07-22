#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Sequential counterpart to GherkinLibraryConcurrentLending.cpp: ONE
// StepRegistry, built once, run against SEVERAL .feature files in the same
// domain (bakery), one after another in the SAME thread - no std::async, no
// CollectingFailureHandler needed for thread-safety, since only one
// scenario is ever running at a time. Just repeated Feature(...).Run()
// calls whose FeatureResults are aggregated into one report at the end.
//
// Only already-passing bakery features are combined here - never
// bakery_late_cancellation.feature, which is a deliberate failure demo (see
// .github/workflows/ci.yml's allow-list comment): mixing an expected-fail
// feature into this single binary would flip this example's own pass/fail
// expectation out from under the CI shell loop.
//
// Registry-sharing note (why these four, and not others):
//   - bakery_standard_order.feature and bakery_allergen_substitution.feature
//     both run against the SAME shared BakerySteps.hpp vocabulary
//     (MakeBakeryStepRegistry()), plus the same kitchen-certification extra
//     step GherkinBakeryAllergenSubstitution.cpp adds via Merge().
//   - bakery_bulk_order_itemized.feature and bakery_flaky_oven_sensor_retry.feature
//     use their OWN, file-specific step vocabularies (see
//     GherkinBakeryBulkOrderItemized.cpp/GherkinBakeryFlakyOvenSensorRetry.cpp)
//     with no shared header of their own - those step definitions are
//     duplicated below (not extracted into a new shared header) to keep
//     this example additive and self-contained, exactly mirroring each
//     standalone example's own local PrepareRegistry().
//   - Every pattern text below is distinct across all four domains, so
//     merging all of them into ONE registry is safe: StepRegistry::TryMatch's
//     first-match-wins linear scan never has to arbitrate between two
//     definitions for the same keyword+pattern.
//   - bakery_seasonal_discount_tiers.feature is deliberately NOT included:
//     its own PrepareRegistry() registers a CONFLICTING "Given a new custom
//     cake order"/"the cake is a {string} cake serving {int} guests" pair
//     with DIFFERENT semantics than BakerySteps.hpp's own (different
//     context keys entirely) - merging it in here would silently shadow
//     one definition behind the other depending on registration order.
//   - bakery_daily_oven_lifecycle.feature is deliberately NOT included: its
//     AddBeforeAllHook/AddAfterAllHook oven-preheat hooks are documented to
//     run once per Feature run - merging them into a registry reused for
//     three unrelated features would fire the oven-preheat/cooldown
//     narration before and after every one of them too, which isn't what
//     "once for this feature" is supposed to mean.

namespace {

// ---- Bulk order itemized: file-specific steps (duplicated from
// GherkinBakeryBulkOrderItemized.cpp - see the rationale above) ----

constexpr double kEpsilon = 0.01;
constexpr Key<double> kOrderTotal{"order_total"};
constexpr Key<std::vector<std::string>> kLineItems{"line_items"};

bool GivenBulkOrderForWeddingCeremony(TestContext& ctx) {
    ctx.Set(kOrderTotal, 0.0);
    ctx.Set(kLineItems, std::vector<std::string>{});
    return true;
}

bool WhenOrderContainsFollowingItems(TestContext& ctx, const DataTable& table) {
    double total = 0.0;
    std::vector<std::string> items;
    for (std::size_t i = 0; i < table.RowCount(); ++i) {
        items.push_back(table.Get(i, "item"));
        total += std::stoi(table.Get(i, "quantity")) * std::stod(table.Get(i, "unit_price"));
    }
    ctx.Set(kOrderTotal, total);
    ctx.Set(kLineItems, std::move(items));
    return true;
}

bool ThenOrderTotalShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kOrderTotal) - expected) < kEpsilon;
}

bool AndOrderShouldHaveLineItems(TestContext& ctx, int expected) {
    return static_cast<int>(ctx.Get(kLineItems).size()) == expected;
}

// ---- Flaky oven sensor retry: file-specific steps (duplicated from
// GherkinBakeryFlakyOvenSensorRetry.cpp - see the rationale above) ----

constexpr Key<double> kOvenTemperature{"oven_temperature"};
constexpr Key<bool> kSensorStable{"sensor_stable"};
constexpr Key<int> kSensorReadAttempts{"sensor_read_attempts"};

bool GivenOvenIsWarmingUp(TestContext& ctx) {
    ctx.Set(kOvenTemperature, 0.0);
    ctx.Set(kSensorStable, false);
    ctx.Set(kSensorReadAttempts, 0);
    return true;
}

bool WhenReadOvenTemperature(TestContext& ctx) {
    static int read_attempt = 0; // process-lifetime counter - see the standalone example's doc comment
    ++read_attempt;
    ctx.Mutate(kSensorReadAttempts) += 1;
    if (read_attempt < 3) {
        return false;
    }
    ctx.Set(kOvenTemperature, 350.0);
    ctx.Set(kSensorStable, true);
    return true;
}

bool ThenTemperatureReadingShouldBeStableAndInRange(TestContext& ctx) {
    return ctx.Get(kSensorStable) && ctx.Get(kOvenTemperature) >= 300.0 && ctx.Get(kOvenTemperature) <= 400.0;
}

// ---- Allergen substitution's own Merge()-extra (duplicated from
// GherkinBakeryAllergenSubstitution.cpp) ----

bool AndKitchenConfirmsSubstitutionIsNutFree(TestContext& ctx, std::string note) {
    return note == "nut-free" && !ctx.Get(bakery_steps::kAllergens).empty();
}

StepRegistry PrepareRegistry() {
    StepRegistry registry = MakeBakeryStepRegistry();

    StepRegistry kitchenExtras;
    kitchenExtras.RegisterAnd("the kitchen confirms the substitution is {word}", AndKitchenConfirmsSubstitutionIsNutFree);
    registry.Merge(kitchenExtras);

    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a bulk order for a wedding ceremony", GivenBulkOrderForWeddingCeremony},
        StepEntry{Keyword::When, "the order contains the following items:", WhenOrderContainsFollowingItems},
        StepEntry{Keyword::Then, "the order total should be {float}", ThenOrderTotalShouldBe},
        StepEntry{Keyword::And, "the order should have {int} line items", AndOrderShouldHaveLineItems},
        StepEntry{Keyword::Given, "the oven is warming up", GivenOvenIsWarmingUp},
        StepEntry{Keyword::When, "I read the oven temperature", WhenReadOvenTemperature},
        StepEntry{Keyword::Then, "the temperature reading should be stable and within range",
            ThenTemperatureReadingShouldBeStableAndInRange});

    return registry;
}

} // namespace

int main() {
    StepRegistry registry = PrepareRegistry();

    constexpr std::array<std::string_view, 4> kFeatureFiles{
        "bakery_standard_order.feature",
        "bakery_allergen_substitution.feature",
        "bakery_bulk_order_itemized.feature",
        "bakery_flaky_oven_sensor_retry.feature",
    };

    std::vector<FeatureResult> results;
    results.reserve(kFeatureFiles.size());
    for (const std::string_view featureFile : kFeatureFiles) {
        std::string featureText = ::LoadFeatureFile(std::string(featureFile));
        const std::string label = std::string("examples/gherkin/features/") + std::string(featureFile);
        results.push_back(Feature(std::move(featureText), registry).Label(label).Run());
    }

    std::cout << "\n=== Bakery Sequential Multi-Feature Run ===\n";
    bool allPassed = true;
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        std::cout << "  " << kFeatureFiles[i] << ": " << result.featureName << " -> "
                  << (result.allPassed ? "PASS" : "FAIL") << " (" << result.scenarioResults.size()
                  << " scenario(s))\n";
        allPassed = allPassed && result.allPassed;
    }
    std::cout << (allPassed ? "All features passed.\n" : "At least one feature failed.\n");

    return allPassed ? 0 : 1;
}
