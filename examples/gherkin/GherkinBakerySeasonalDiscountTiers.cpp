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

StepRegistry PrepareRegistry() {
    StepRegistry registry;

    constexpr double kEpsilon = 0.01;

    // ---- Order setup ----
    registry.RegisterGiven("a new custom cake order", [](TestContext& ctx) -> bool {
        ctx.Set("tier_name", std::string(""));
        ctx.Set("order_total", 0.0);
        ctx.Set("discount_percentage", 0.0);
        ctx.Set("final_price", 0.0);
        return true;
    });

    // ---- Loyalty tier setup ----
    registry.RegisterAnd("the customer has loyalty tier {string}", [](TestContext& ctx, std::string tier) -> bool {
        ctx.Set("tier_name", tier);
        return true;
    });

    // ---- Cake and order total ----
    auto setCakeFn = [](TestContext& ctx, std::string flavor, int servings) -> bool {
        ctx.Set("flavor", flavor);
        ctx.Set("servings", servings);
        // Price calculation: $2.50 per serving (consistent with BakerySteps.hpp)
        double base_price = 2.50 * servings;
        ctx.Set("base_price", base_price);
        return true;
    };
    registry.RegisterAnd("the cake is a {string} cake serving {int} guests", setCakeFn);

    registry.RegisterWhen("the order total is {float}", [](TestContext& ctx, double total) -> bool {
        ctx.Set("order_total", total);

        // Apply discount based on tier
        std::string tier = ctx.Get<std::string>("tier_name");
        double discount_pct = 0.0;

        if (tier == "bronze") {
            discount_pct = 0.0;
        } else if (tier == "silver") {
            discount_pct = 5.0;
        } else if (tier == "gold") {
            discount_pct = 10.0;
        } else if (tier == "platinum") {
            discount_pct = 15.0;
        }

        ctx.Set("discount_percentage", discount_pct);

        // Calculate final price
        double discount_amount = (total * discount_pct) / 100.0;
        double final = total - discount_amount;
        ctx.Set("final_price", final);

        return true;
    });

    // ---- Assertions ----
    registry.RegisterThen("the discount percentage should be {float}",
        [](TestContext& ctx, double expected) -> bool {
            double actual = ctx.Get<double>("discount_percentage");
            return std::abs(actual - expected) < kEpsilon;
        });

    registry.RegisterAnd("the final price should be {float}", [](TestContext& ctx, double expected) -> bool {
        double actual = ctx.Get<double>("final_price");
        return std::abs(actual - expected) < kEpsilon;
    });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_seasonal_discount_tiers.feature");
}
