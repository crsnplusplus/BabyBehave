#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <iostream>
#include <string>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Suite-level Before/After hook example (Feature 8):
// demonstrates AddBeforeAllHook() and AddAfterAllHook() to run
// setup/teardown actions exactly ONCE for the entire feature,
// regardless of how many scenarios exist.
//
// The bakery oven must be preheated once before ANY scenario runs,
// and cooled down/inspected once after ALL scenarios finish. Each
// scenario's "the oven is ready" Given step asserts that the preheat
// has already occurred, proving the Before-ALL hook fired at the
// feature level, not per-scenario.

namespace {

// Static flag to track whether oven has been preheated for the entire day
bool oven_preheated_for_day = false;

constexpr Key<std::string> kBakedItem{"baked_item"};

// ---- Oven readiness check ----
// This Given step verifies that the oven is already preheated
// (the Before-ALL hook should have set oven_preheated_for_day=true
// before any scenario's steps run).
bool GivenOvenIsReady(TestContext& ctx) {
    if (!oven_preheated_for_day) {
        std::cerr << "  [ERROR] Oven not preheated - Before-ALL hook did not run!\n";
        return false;
    }
    std::cout << "  [Given] Oven confirmed ready and preheated\n";
    return true;
}

// ---- Baking steps (croissants) ----
bool WhenBakeCroissants(TestContext& ctx) {
    std::cout << "  [When] Baking croissants in preheated oven\n";
    ctx.Set(kBakedItem, std::string("croissants"));
    return true;
}

bool ThenCroissantsShouldBeGoldenBrown(TestContext& ctx) {
    const std::string item = ctx.Get(kBakedItem);
    std::cout << "  [Then] Croissants are golden brown\n";
    return item == "croissants";
}

// ---- Baking steps (baguettes) ----
bool WhenBakeBaguettes(TestContext& ctx) {
    std::cout << "  [When] Baking baguettes in preheated oven\n";
    ctx.Set(kBakedItem, std::string("baguettes"));
    return true;
}

bool ThenBaguettesShouldHaveCrispCrust(TestContext& ctx) {
    const std::string item = ctx.Get(kBakedItem);
    std::cout << "  [Then] Baguettes have crisp crust\n";
    return item == "baguettes";
}

// ---- Baking steps (cookies) ----
bool WhenBakeCookies(TestContext& ctx) {
    std::cout << "  [When] Baking cookies in preheated oven\n";
    ctx.Set(kBakedItem, std::string("cookies"));
    return true;
}

bool ThenCookiesShouldBeEvenlyBaked(TestContext& ctx) {
    const std::string item = ctx.Get(kBakedItem);
    std::cout << "  [Then] Cookies are evenly baked\n";
    return item == "cookies";
}

// ---- Before-ALL hook: Preheat oven once for entire day ----
// This hook runs exactly ONCE before ANY scenario in this feature
// executes, regardless of how many scenarios there are (3 in this case).
// It sets oven_preheated_for_day = true so subsequent Given steps
// can verify it.
void PreheatOvenForDay() {
    std::cout << "\n[BEFORE-ALL] Preheating oven for the entire day...\n";
    oven_preheated_for_day = true;
    std::cout << "[BEFORE-ALL] Oven is now preheated and ready\n\n";
}

// ---- After-ALL hook: Cool down and inspect oven once after all scenarios ----
// This hook runs exactly ONCE after ALL scenarios in this feature have
// finished (passed or failed), regardless of how many scenarios there are.
// It logs the end-of-day inspection.
void CooldownOvenForDay() {
    std::cout << "\n[AFTER-ALL] Beginning end-of-day oven cooldown...\n";
    std::cout << "[AFTER-ALL] Inspecting oven: all baking complete\n";
    oven_preheated_for_day = false; // Reset for next day
    std::cout << "[AFTER-ALL] Oven cooled down and secured for the day\n\n";
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry;
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "the oven is ready", GivenOvenIsReady},
        StepEntry{Keyword::When, "I bake croissants", WhenBakeCroissants},
        StepEntry{Keyword::Then, "the croissants should be golden brown", ThenCroissantsShouldBeGoldenBrown},
        StepEntry{Keyword::When, "I bake baguettes", WhenBakeBaguettes},
        StepEntry{Keyword::Then, "the baguettes should have a crisp crust", ThenBaguettesShouldHaveCrispCrust},
        StepEntry{Keyword::When, "I bake cookies", WhenBakeCookies},
        StepEntry{Keyword::Then, "the cookies should be evenly baked", ThenCookiesShouldBeEvenlyBaked});

    // Suite-level Before-ALL/After-ALL hooks (Feature 8): AddAroundHook does
    // NOT apply here - it pairs per-Scenario, tag-filtered Before/After hooks
    // that share one tag filter/expression, whereas AddBeforeAllHook/
    // AddAfterAllHook are unconditional, feature-wide, and take no tags/
    // expression parameter at all (see AddBeforeAllHook's doc comment in
    // bdd.hpp) - there is no equivalent "AddAroundAllHook" to reach for.
    registry.AddBeforeAllHook(PreheatOvenForDay);
    registry.AddAfterAllHook(CooldownOvenForDay);

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_daily_oven_lifecycle.feature");
}
