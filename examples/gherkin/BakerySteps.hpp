#ifndef BABYBEHAVE_EXAMPLES_GHERKIN_BAKERY_STEPS_HPP
#define BABYBEHAVE_EXAMPLES_GHERKIN_BAKERY_STEPS_HPP

#include <BabyBehave/bdd.hpp>
#include <cmath>
#include <string>
#include <vector>

// Shared step-definition "library" for a small bakery's custom cake
// ordering system: pricing by servings, allergen substitutions (with a
// surcharge), deposits, and a cancellation policy with a deposit-forfeiture
// grace period. This header is included by every GherkinBakery*.cpp example
// under examples/gherkin/ - each builds its own StepRegistry from
// MakeBakeryStepRegistry() and runs a different, realistic scenario against
// the same ~12 shared step definitions, demonstrating the "define once,
// reuse across many tests" pattern documented in
// docs/design/gherkin-support.md's Merge()/reuse section.
//
// Not part of the BabyBehave library itself - this is example-only code,
// so the `using namespace` directives below are intentionally scoped to
// this header's small, self-contained audience (files under examples/gherkin/).

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

inline StepRegistry MakeBakeryStepRegistry() {
    StepRegistry registry;

    constexpr double kPricePerServing = 2.50;
    constexpr double kAllergenSurcharge = 5.00;
    constexpr int kCancellationGraceDays = 3;
    constexpr double kEpsilon = 0.01;

    // ---- Order setup ----
    registry.RegisterGiven("a new custom cake order", [](TestContext& ctx) -> bool {
        ctx.Set("price", 0.0);
        ctx.Set("deposit_paid", 0.0);
        ctx.Set("allergens", std::vector<std::string>{});
        ctx.Set("cancelled", false);
        ctx.Set("deposit_forfeited", false);
        return true;
    });

    auto setCakeFn = [](TestContext& ctx, std::string flavor, int servings) -> bool {
        ctx.Set("flavor", flavor);
        ctx.Set("servings", servings);
        ctx.Set("price", kPricePerServing * servings);
        return true;
    };
    registry.RegisterGiven("the cake is a {string} cake serving {int} guests", setCakeFn);
    registry.RegisterAnd("the cake is a {string} cake serving {int} guests", setCakeFn);

    // ---- Allergen substitutions (add a per-substitution surcharge) ----
    auto allergenSubstitutionFn = [](TestContext& ctx, std::string allergen) -> bool {
        auto allergens = ctx.Get<std::vector<std::string>>("allergens");
        allergens.push_back(allergen);
        ctx.Set("allergens", allergens);
        ctx.Set("price", ctx.Get<double>("price") + kAllergenSurcharge);
        return true;
    };
    registry.RegisterWhen("the customer requests a {word} allergen substitution", allergenSubstitutionFn);
    registry.RegisterAnd("the customer requests a {word} allergen substitution", allergenSubstitutionFn);

    // ---- Deposits ----
    auto payDepositFn = [](TestContext& ctx, double amount) -> bool {
        ctx.Set("deposit_paid", ctx.Get<double>("deposit_paid") + amount);
        return true;
    };
    registry.RegisterWhen("a deposit of {float} is paid", payDepositFn);
    registry.RegisterAnd("a deposit of {float} is paid", payDepositFn);

    // ---- Cancellation policy ----
    registry.RegisterWhen("the order is cancelled {int} days before pickup",
        [](TestContext& ctx, int daysBeforePickup) -> bool {
            ctx.Set("cancelled", true);
            ctx.Set("deposit_forfeited", daysBeforePickup < kCancellationGraceDays);
            return true;
        });

    // ---- Assertions ----
    registry.RegisterThen("the total price should be {float}", [](TestContext& ctx, double expected) -> bool {
        return std::abs(ctx.Get<double>("price") - expected) < kEpsilon;
    });

    auto depositPaidCheckFn = [](TestContext& ctx, double expected) -> bool {
        return std::abs(ctx.Get<double>("deposit_paid") - expected) < kEpsilon;
    };
    registry.RegisterThen("the deposit paid should be {float}", depositPaidCheckFn);
    registry.RegisterAnd("the deposit paid should be {float}", depositPaidCheckFn);

    auto depositForfeitedFn = [](TestContext& ctx) -> bool { return ctx.Get<bool>("deposit_forfeited"); };
    registry.RegisterThen("the deposit should be forfeited", depositForfeitedFn);
    registry.RegisterAnd("the deposit should be forfeited", depositForfeitedFn);

    registry.RegisterThen("the deposit should be refunded", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("cancelled") && !ctx.Get<bool>("deposit_forfeited");
    });
    registry.RegisterBut("the deposit should be refunded", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("cancelled") && !ctx.Get<bool>("deposit_forfeited");
    });

    registry.RegisterAnd("the order should list {int} allergen substitution", [](TestContext& ctx, int expected) -> bool {
        return static_cast<int>(ctx.Get<std::vector<std::string>>("allergens").size()) == expected;
    });

    registry.RegisterBut("no allergen substitutions should be recorded", [](TestContext& ctx) -> bool {
        return ctx.Get<std::vector<std::string>>("allergens").empty();
    });

    return registry;
}

#endif // BABYBEHAVE_EXAMPLES_GHERKIN_BAKERY_STEPS_HPP
