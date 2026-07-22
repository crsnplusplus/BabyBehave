#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

constexpr Key<bool> kDbConnected{"db_connected"};
constexpr Key<bool> kQueryResult{"query_result"};
constexpr Key<bool> kSetupRan{"setup_ran"};
constexpr Key<bool> kCleanupRan{"cleanup_ran"};

bool GivenDatabaseConnection(TestContext& ctx) {
    ctx.Set(kDbConnected, true);
    // Don't override setup_ran here - let the hook set it
    return true;
}

bool WhenQueryDatabase(TestContext& ctx) {
    ctx.Set(kQueryResult, ctx.Get(kDbConnected));
    return true;
}

bool ThenQueryShouldSucceed(TestContext& ctx) {
    return ctx.Get(kQueryResult);
}

bool SetupHookShouldHaveRun(TestContext& ctx) {
    try {
        return ctx.Get(kSetupRan);
    } catch (...) {
        // If the key doesn't exist, return false
        return false;
    }
}

// Before hook for @slow tag: marks that a setup hook ran.
void MarkSetupHookRan(TestContext& ctx) {
    ctx.Set(kSetupRan, true);
}

// After hook (runs for all scenarios since tags are empty): cleanup.
//
// Note: this After hook's tag filter ({}, i.e. "all scenarios") is
// deliberately DIFFERENT from the Before hook's ({"slow"}) - that
// contrast (tag-scoped setup vs. unconditional cleanup) is the whole
// point of this example, so AddAroundHook (which requires the SAME tag
// filter for both halves) does not apply here.
void MarkCleanupRan(TestContext& ctx) {
    ctx.Set(kCleanupRan, true);
}

} // namespace

int main() {
    StepRegistry registry;

    // Register step definitions
    registry.RegisterGiven("a database connection", GivenDatabaseConnection);
    registry.RegisterWhen("I query the database", WhenQueryDatabase);
    registry.RegisterThen("the query should succeed", ThenQueryShouldSucceed);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "setup hook should have run", SetupHookShouldHaveRun);

    registry.AddBeforeHook({"slow"}, MarkSetupHookRan);
    registry.AddAfterHook({}, MarkCleanupRan);

    const std::string_view feature = R"feature(
Feature: Database operations with hooks
  @slow
  Scenario: Query with before hook
    Given a database connection
    When I query the database
    Then the query should succeed
    And setup hook should have run

  @fast
  Scenario: Query without before hook
    Given a database connection
    When I query the database
    Then the query should succeed
)feature";

    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinTagsAndHooks.cpp").Run();
    return result.ExitCode();
}
