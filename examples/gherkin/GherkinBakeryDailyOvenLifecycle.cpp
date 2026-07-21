#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
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

// Static flag to track whether oven has been preheated for the entire day
static bool oven_preheated_for_day = false;

StepRegistry PrepareRegistry() {
    StepRegistry registry;

    // ---- Oven readiness check ----
    // This Given step verifies that the oven is already preheated
    // (the Before-ALL hook should have set oven_preheated_for_day=true
    // before any scenario's steps run).
    registry.RegisterGiven("the oven is ready", [](TestContext& ctx) -> bool {
        if (!oven_preheated_for_day) {
            std::cerr << "  [ERROR] Oven not preheated - Before-ALL hook did not run!\n";
            return false;
        }
        std::cout << "  [Given] Oven confirmed ready and preheated\n";
        return true;
    });

    // ---- Baking steps (croissants) ----
    registry.RegisterWhen("I bake croissants", [](TestContext& ctx) -> bool {
        std::cout << "  [When] Baking croissants in preheated oven\n";
        ctx.Set("baked_item", std::string("croissants"));
        return true;
    });

    registry.RegisterThen("the croissants should be golden brown", [](TestContext& ctx) -> bool {
        std::string item = ctx.Get<std::string>("baked_item");
        std::cout << "  [Then] Croissants are golden brown\n";
        return item == "croissants";
    });

    // ---- Baking steps (baguettes) ----
    registry.RegisterWhen("I bake baguettes", [](TestContext& ctx) -> bool {
        std::cout << "  [When] Baking baguettes in preheated oven\n";
        ctx.Set("baked_item", std::string("baguettes"));
        return true;
    });

    registry.RegisterThen("the baguettes should have a crisp crust", [](TestContext& ctx) -> bool {
        std::string item = ctx.Get<std::string>("baked_item");
        std::cout << "  [Then] Baguettes have crisp crust\n";
        return item == "baguettes";
    });

    // ---- Baking steps (cookies) ----
    registry.RegisterWhen("I bake cookies", [](TestContext& ctx) -> bool {
        std::cout << "  [When] Baking cookies in preheated oven\n";
        ctx.Set("baked_item", std::string("cookies"));
        return true;
    });

    registry.RegisterThen("the cookies should be evenly baked", [](TestContext& ctx) -> bool {
        std::string item = ctx.Get<std::string>("baked_item");
        std::cout << "  [Then] Cookies are evenly baked\n";
        return item == "cookies";
    });

    // ---- Before-ALL hook: Preheat oven once for entire day ----
    // This hook runs exactly ONCE before ANY scenario in this feature
    // executes, regardless of how many scenarios there are (3 in this case).
    // It sets oven_preheated_for_day = true so subsequent Given steps
    // can verify it.
    registry.AddBeforeAllHook([]() {
        std::cout << "\n[BEFORE-ALL] Preheating oven for the entire day...\n";
        oven_preheated_for_day = true;
        std::cout << "[BEFORE-ALL] Oven is now preheated and ready\n\n";
    });

    // ---- After-ALL hook: Cool down and inspect oven once after all scenarios ----
    // This hook runs exactly ONCE after ALL scenarios in this feature have
    // finished (passed or failed), regardless of how many scenarios there are.
    // It logs the end-of-day inspection.
    registry.AddAfterAllHook([]() {
        std::cout << "\n[AFTER-ALL] Beginning end-of-day oven cooldown...\n";
        std::cout << "[AFTER-ALL] Inspecting oven: all baking complete\n";
        oven_preheated_for_day = false;  // Reset for next day
        std::cout << "[AFTER-ALL] Oven cooled down and secured for the day\n\n";
    });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_daily_oven_lifecycle.feature");
}
