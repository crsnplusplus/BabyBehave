#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <string>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Tag expression example: demonstrates AddBeforeHookExpr() with a boolean
// expression (@vip or @urgent) to enable priority patron handling in a library.
// 
// The Before hook fires ONLY for scenarios tagged @vip OR @urgent, setting
// a priority_mode flag that downstream steps check. The plain scenario (no tags)
// does NOT match the expression, so the hook does not fire, and standard
// processing applies.
//
// This is Feature 7 (tag expressions AND/OR/NOT) applied to Before hooks:
// a more flexible, expression-based alternative to the vector-based tag list
// in AddBeforeHook() for managing complex tag combinations.

StepRegistry PrepareRegistry() {
    StepRegistry registry;

    // ---- Patron system setup ----
    // priority_mode is guaranteed to be set by an unconditional Before hook that runs first.
    registry.RegisterGiven("a library patron management system", [](TestContext& ctx) -> bool {
        // priority_mode already initialized by Before hook; nothing to do here
        return true;
    });

    // ---- Request initiation ----
    registry.RegisterGiven("a patron requests a book", [](TestContext& ctx) -> bool {
        ctx.Set("request_status", std::string("pending"));
        return true;
    });

    // ---- Processing ----
    registry.RegisterWhen("the request is processed", [](TestContext& ctx) -> bool {
        ctx.Set("request_status", "completed");
        return true;
    });

    // ---- Assertions ----
    registry.RegisterThen("the patron should receive expedited service", [](TestContext& ctx) -> bool {
        // Expedited service required: priority_mode must be true
        return ctx.Get<bool>("priority_mode");
    });

    registry.RegisterThen("the patron should receive standard service", [](TestContext& ctx) -> bool {
        // Standard service required: priority_mode must be false
        return !ctx.Get<bool>("priority_mode");
    });

    // ---- Before hooks ----
    // Unconditional Before hook: always run first, sets default priority_mode=false
    // (runs before the tag-expression hook in registration order).
    // Empty tag vector means this hook runs for all scenarios.
    registry.AddBeforeHook({}, [](TestContext& ctx) {
        ctx.Set("priority_mode", false);
    });

    // Tag-expression Before hook: overrides to priority_mode=true for @vip/@urgent.
    // This hook fires if the scenario is tagged @vip OR @urgent (case-insensitive).
    // The expression syntax supports @tag, and, or, not, and parentheses.
    // Scenarios without @vip and without @urgent never trigger this, so they keep
    // the false value set by the unconditional hook above.
    registry.AddBeforeHookExpr("@vip or @urgent", [](TestContext& ctx) {
        std::cout << "  [Before] VIP/urgent patron detected - enabling priority mode\n";
        ctx.Set("priority_mode", true);
    });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "library_priority_patron_handling.feature");
}
