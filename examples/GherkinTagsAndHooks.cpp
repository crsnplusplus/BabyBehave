#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    // Register step definitions
    registry.RegisterGiven("a database connection", [](TestContext& ctx) -> bool {
        ctx.Set("db_connected", true);
        // Don't override setup_ran here - let the hook set it
        return true;
    });

    registry.RegisterWhen("I query the database", [](TestContext& ctx) -> bool {
        bool db_connected = ctx.Get<bool>("db_connected");
        ctx.Set("query_result", db_connected);
        return true;
    });

    registry.RegisterThen("the query should succeed", [](TestContext& ctx) -> bool {
        bool result = ctx.Get<bool>("query_result");
        return result;
    });

    auto setupHookRanFn = [](TestContext& ctx) -> bool {
        try {
            bool setup_ran = ctx.Get<bool>("setup_ran");
            return setup_ran;
        } catch (...) {
            // If the key doesn't exist, return false
            return false;
        }
    };

    registry.RegisterThen("setup hook should have run", setupHookRanFn);
    registry.RegisterAnd("setup hook should have run", setupHookRanFn);

    // Register Before hook for @slow tag
    registry.AddBeforeHook(
        {"slow"},
        [](TestContext& ctx) {
            // Mark that a setup hook ran
            ctx.Set("setup_ran", true);
        }
    );

    // Register After hook (runs for all scenarios since tags are empty)
    registry.AddAfterHook(
        {},
        [](TestContext& ctx) {
            // Cleanup - set a flag to verify this ran
            ctx.Set("cleanup_ran", true);
        }
    );

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

    const auto result = RunFeature(feature, registry, "examples/GherkinTagsAndHooks.cpp");
    return result.allPassed ? 0 : 1;
}
