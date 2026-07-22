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
// Organized as one small RegisterBakery*Steps(registry) function per
// sub-domain (order setup, allergens, deposits, cancellation), each
// registering 2-3 related step patterns, composed by MakeBakeryStepRegistry()
// below - a scannable table of "what this domain teaches" rather than one
// long flat function. Per-file Merge() extension (see
// GherkinBakeryAllergenSubstitution.cpp) still works exactly as before:
// MakeBakeryStepRegistry() returns a plain StepRegistry value like always.
//
// Not part of the BabyBehave library itself - this is example-only code,
// so the `using namespace` directives below are intentionally scoped to
// this header's small, self-contained audience (files under examples/gherkin/).

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace bakery_steps {

inline constexpr double kPricePerServing = 2.50;
inline constexpr double kAllergenSurcharge = 5.00;
inline constexpr int kCancellationGraceDays = 3;
inline constexpr double kEpsilon = 0.01;

// Typed context keys (Key<T> = TestContext::ContextKey<T>): catches a
// key/type mismatch at the ctx.Get<T>/Mutate<T> call site at compile time
// instead of a std::bad_any_cast at run time, and reads as a small
// vocabulary of "what this domain stores" up front.
inline constexpr Key<double> kPrice{"price"};
inline constexpr Key<double> kDepositPaid{"deposit_paid"};
inline constexpr Key<std::vector<std::string>> kAllergens{"allergens"};
inline constexpr Key<bool> kCancelled{"cancelled"};
inline constexpr Key<bool> kDepositForfeited{"deposit_forfeited"};
inline constexpr Key<std::string> kFlavor{"flavor"};
inline constexpr Key<int> kServings{"servings"};

// ---- Order setup ----

inline bool GivenNewCustomCakeOrder(TestContext& ctx) {
    ctx.Set(kPrice, 0.0);
    ctx.Set(kDepositPaid, 0.0);
    ctx.Set(kAllergens, std::vector<std::string>{});
    ctx.Set(kCancelled, false);
    ctx.Set(kDepositForfeited, false);
    return true;
}

inline bool GivenSetCakeFlavorAndServings(TestContext& ctx, std::string flavor, int servings) {
    ctx.Set(kFlavor, std::move(flavor));
    ctx.Set(kServings, servings);
    ctx.Set(kPrice, kPricePerServing * servings);
    return true;
}

inline bool ThenTotalPriceShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kPrice) - expected) < kEpsilon;
}

inline void RegisterBakeryOrderSetupSteps(StepRegistry& registry) {
    registry.RegisterGiven("a new custom cake order", GivenNewCustomCakeOrder);
    registry.RegisterStep({Keyword::Given, Keyword::And}, "the cake is a {string} cake serving {int} guests",
        GivenSetCakeFlavorAndServings);
    registry.RegisterThen("the total price should be {float}", ThenTotalPriceShouldBe);
}

// ---- Allergen substitutions (add a per-substitution surcharge) ----

inline bool WhenRequestAllergenSubstitution(TestContext& ctx, std::string allergen) {
    ctx.Mutate(kAllergens).push_back(std::move(allergen));
    ctx.Mutate(kPrice) += kAllergenSurcharge;
    return true;
}

inline bool AndOrderShouldListAllergenSubstitutions(TestContext& ctx, int expected) {
    return static_cast<int>(ctx.Get(kAllergens).size()) == expected;
}

inline bool ButNoAllergenSubstitutionsRecorded(TestContext& ctx) {
    return ctx.Get(kAllergens).empty();
}

inline void RegisterBakeryAllergenSteps(StepRegistry& registry) {
    registry.RegisterStep({Keyword::When, Keyword::And}, "the customer requests a {word} allergen substitution",
        WhenRequestAllergenSubstitution);
    registry.RegisterAnd("the order should list {int} allergen substitution", AndOrderShouldListAllergenSubstitutions);
    registry.RegisterBut("no allergen substitutions should be recorded", ButNoAllergenSubstitutionsRecorded);
}

// ---- Deposits ----

inline bool WhenPayDeposit(TestContext& ctx, double amount) {
    ctx.Mutate(kDepositPaid) += amount;
    return true;
}

inline bool ThenDepositPaidShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kDepositPaid) - expected) < kEpsilon;
}

inline void RegisterBakeryDepositSteps(StepRegistry& registry) {
    registry.RegisterStep({Keyword::When, Keyword::And}, "a deposit of {float} is paid", WhenPayDeposit);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the deposit paid should be {float}", ThenDepositPaidShouldBe);
}

// ---- Cancellation policy ----

inline bool WhenCancelOrder(TestContext& ctx, int daysBeforePickup) {
    ctx.Set(kCancelled, true);
    ctx.Set(kDepositForfeited, daysBeforePickup < kCancellationGraceDays);
    return true;
}

inline bool ThenDepositShouldBeForfeited(TestContext& ctx) {
    return ctx.Get(kDepositForfeited);
}

inline bool ThenDepositShouldBeRefunded(TestContext& ctx) {
    return ctx.Get(kCancelled) && !ctx.Get(kDepositForfeited);
}

inline void RegisterBakeryCancellationSteps(StepRegistry& registry) {
    registry.RegisterWhen("the order is cancelled {int} days before pickup", WhenCancelOrder);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the deposit should be forfeited", ThenDepositShouldBeForfeited);
    registry.RegisterStep({Keyword::Then, Keyword::But}, "the deposit should be refunded", ThenDepositShouldBeRefunded);
}

} // namespace bakery_steps

inline StepRegistry MakeBakeryStepRegistry() {
    StepRegistry registry;
    bakery_steps::RegisterBakeryOrderSetupSteps(registry);
    bakery_steps::RegisterBakeryAllergenSteps(registry);
    bakery_steps::RegisterBakeryDepositSteps(registry);
    bakery_steps::RegisterBakeryCancellationSteps(registry);
    return registry;
}

#endif // BABYBEHAVE_EXAMPLES_GHERKIN_BAKERY_STEPS_HPP
