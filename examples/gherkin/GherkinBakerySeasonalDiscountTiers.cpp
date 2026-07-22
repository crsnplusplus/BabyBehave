#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <cmath>
#include <string>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Scenario Outline example: seasonal discount tiers for bakery loyalty
// customers. Demonstrates how a Scenario Outline with <placeholder> tokens
// in step text, followed by an Examples: table with pipe-delimited data rows,
// is expanded at parse time into one ordinary Scenario per data row, with
// placeholder tokens substituted from the corresponding row's cells.
//
// The scenario text lives in examples/gherkin/features/
// bakery_seasonal_discount_tiers.feature, read from disk via
// LoadFeatureFile() - see GherkinBakeryStandardOrder.cpp's comment for why.

namespace {

constexpr double kEpsilon = 0.01;

constexpr Key<std::string> kTierName{"tier_name"};
constexpr Key<double> kOrderTotal{"order_total"};
constexpr Key<double> kDiscountPercentage{"discount_percentage"};
constexpr Key<double> kFinalPrice{"final_price"};
constexpr Key<std::string> kFlavor{"flavor"};
constexpr Key<int> kServings{"servings"};
constexpr Key<double> kBasePrice{"base_price"};

// ---- Order setup ----
bool GivenNewCustomCakeOrder(TestContext& ctx) {
    ctx.Set(kTierName, std::string(""));
    ctx.Set(kOrderTotal, 0.0);
    ctx.Set(kDiscountPercentage, 0.0);
    ctx.Set(kFinalPrice, 0.0);
    return true;
}

// ---- Loyalty tier setup ----
bool AndCustomerHasLoyaltyTier(TestContext& ctx, std::string tier) {
    ctx.Set(kTierName, std::move(tier));
    return true;
}

// ---- Cake and order total ----
bool AndSetCakeFlavorAndServings(TestContext& ctx, std::string flavor, int servings) {
    ctx.Set(kFlavor, std::move(flavor));
    ctx.Set(kServings, servings);
    // Price calculation: $2.50 per serving (consistent with BakerySteps.hpp)
    ctx.Set(kBasePrice, 2.50 * servings);
    return true;
}

bool WhenOrderTotalIs(TestContext& ctx, double total) {
    ctx.Set(kOrderTotal, total);

    // Apply discount based on tier
    const std::string tier = ctx.Get(kTierName);
    double discountPct = 0.0;

    if (tier == "bronze") {
        discountPct = 0.0;
    } else if (tier == "silver") {
        discountPct = 5.0;
    } else if (tier == "gold") {
        discountPct = 10.0;
    } else if (tier == "platinum") {
        discountPct = 15.0;
    }

    ctx.Set(kDiscountPercentage, discountPct);

    // Calculate final price
    const double discountAmount = (total * discountPct) / 100.0;
    ctx.Set(kFinalPrice, total - discountAmount);

    return true;
}

// ---- Assertions ----
bool ThenDiscountPercentageShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kDiscountPercentage) - expected) < kEpsilon;
}

bool AndFinalPriceShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kFinalPrice) - expected) < kEpsilon;
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry;
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a new custom cake order", GivenNewCustomCakeOrder},
        StepEntry{Keyword::And, "the customer has loyalty tier {string}", AndCustomerHasLoyaltyTier},
        StepEntry{Keyword::And, "the cake is a {string} cake serving {int} guests", AndSetCakeFlavorAndServings},
        StepEntry{Keyword::When, "the order total is {float}", WhenOrderTotalIs},
        StepEntry{Keyword::Then, "the discount percentage should be {float}", ThenDiscountPercentageShouldBe},
        StepEntry{Keyword::And, "the final price should be {float}", AndFinalPriceShouldBe});
    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_seasonal_discount_tiers.feature");
}
