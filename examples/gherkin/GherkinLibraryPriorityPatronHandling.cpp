#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <iostream>
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

namespace {

constexpr Key<bool> kPriorityMode{"priority_mode"};
constexpr Key<std::string> kRequestStatus{"request_status"};

// ---- Patron system setup ----
// priority_mode is guaranteed to be set by an unconditional Before hook that runs first.
bool GivenLibraryPatronManagementSystem(TestContext& ctx) {
    // priority_mode already initialized by Before hook; nothing to do here
    return true;
}

// ---- Request initiation ----
bool GivenPatronRequestsBook(TestContext& ctx) {
    ctx.Set(kRequestStatus, std::string("pending"));
    return true;
}

// ---- Processing ----
bool WhenRequestIsProcessed(TestContext& ctx) {
    ctx.Set(kRequestStatus, std::string("completed"));
    return true;
}

// ---- Assertions ----
bool ThenPatronShouldReceiveExpeditedService(TestContext& ctx) {
    // Expedited service required: priority_mode must be true
    return ctx.Get(kPriorityMode);
}

bool ThenPatronShouldReceiveStandardService(TestContext& ctx) {
    // Standard service required: priority_mode must be false
    return !ctx.Get(kPriorityMode);
}

// ---- Before hooks ----
// Unconditional Before hook: always run first, sets default priority_mode=false
// (runs before the tag-expression hook in registration order).
void SetDefaultPriorityMode(TestContext& ctx) {
    ctx.Set(kPriorityMode, false);
}

// Tag-expression Before hook: overrides to priority_mode=true for @vip/@urgent.
// This hook fires if the scenario is tagged @vip OR @urgent (case-insensitive).
// Scenarios without @vip and without @urgent never trigger this, so they keep
// the false value set by the unconditional hook above.
//
// Note: this is a Before-only pair with no matching After hook, so
// AddAroundHook (which pairs a Before+After sharing one tag filter/
// expression) doesn't apply here - both hooks below stay as independent
// AddBeforeHook()/AddBeforeHookExpr() registrations.
void EnablePriorityModeForVipOrUrgent(TestContext& ctx) {
    std::cout << "  [Before] VIP/urgent patron detected - enabling priority mode\n";
    ctx.Set(kPriorityMode, true);
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry;
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a library patron management system", GivenLibraryPatronManagementSystem},
        StepEntry{Keyword::Given, "a patron requests a book", GivenPatronRequestsBook},
        StepEntry{Keyword::When, "the request is processed", WhenRequestIsProcessed},
        StepEntry{Keyword::Then, "the patron should receive expedited service", ThenPatronShouldReceiveExpeditedService},
        StepEntry{Keyword::Then, "the patron should receive standard service", ThenPatronShouldReceiveStandardService});

    // Empty tag vector means this hook runs for all scenarios.
    registry.AddBeforeHook({}, SetDefaultPriorityMode);
    // The expression syntax supports @tag, and, or, not, and parentheses.
    registry.AddBeforeHookExpr("@vip or @urgent", EnablePriorityModeForVipOrUrgent);

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "library_priority_patron_handling.feature");
}
