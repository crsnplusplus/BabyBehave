#include <BabyBehave/bdd.hpp>
#include <iostream>
#include <vector>
#include <iomanip>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Advanced Gherkin example combining:
// - Background: common setup shared across scenarios
// - Multiple Scenario with @tags
// - Before/After hooks scoped to tags
// - Multiple placeholder types: {int}, {string}, {float}, {word}
// - And/But step connectors
// - Realistic domain: order processing with validation and pricing

int main() {
    StepRegistry registry;

    // ---- Background: Common order setup ----
    registry.RegisterGiven("an empty order", [](TestContext& ctx) -> bool {
        std::vector<std::string> items;
        ctx.Set("items", items);
        ctx.Set("total_price", 0.0);
        ctx.Set("discount", 0.0);
        return true;
    });

    // ---- Item addition (Given/When/And) ----
    registry.RegisterWhen("I add a {word} for {float}", [](TestContext& ctx, std::string item, double price) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        items.push_back(item + " ($" + std::to_string(price) + ")");
        ctx.Set("items", items);

        double total = ctx.Get<double>("total_price");
        ctx.Set("total_price", total + price);
        return true;
    });

    registry.RegisterAnd("I add a {word} for {float}", [](TestContext& ctx, std::string item, double price) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        items.push_back(item + " ($" + std::to_string(price) + ")");
        ctx.Set("items", items);

        double total = ctx.Get<double>("total_price");
        ctx.Set("total_price", total + price);
        return true;
    });

    // ---- Discount application ----
    registry.RegisterWhen("I apply a {int} percent discount", [](TestContext& ctx, int discountPercent) -> bool {
        double total = ctx.Get<double>("total_price");
        double discountAmount = (total * discountPercent) / 100.0;
        ctx.Set("discount", discountAmount);
        ctx.Set("final_price", total - discountAmount);
        return true;
    });

    registry.RegisterAnd("I apply a {int} percent discount", [](TestContext& ctx, int discountPercent) -> bool {
        double total = ctx.Get<double>("total_price");
        double discountAmount = (total * discountPercent) / 100.0;
        ctx.Set("discount", discountAmount);
        ctx.Set("final_price", total - discountAmount);
        return true;
    });

    // ---- Validation (Then/And/But) ----
    registry.RegisterThen("the order should contain {int} items", [](TestContext& ctx, int expectedCount) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        return static_cast<int>(items.size()) == expectedCount;
    });

    registry.RegisterAnd("the order should contain {int} items", [](TestContext& ctx, int expectedCount) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        return static_cast<int>(items.size()) == expectedCount;
    });

    registry.RegisterThen("the order total should be {float}", [](TestContext& ctx, double expected) -> bool {
        double total = ctx.Get<double>("total_price");
        const double epsilon = 0.01;
        return std::abs(total - expected) < epsilon;
    });

    registry.RegisterAnd("the order total should be {float}", [](TestContext& ctx, double expected) -> bool {
        double total = ctx.Get<double>("total_price");
        const double epsilon = 0.01;
        return std::abs(total - expected) < epsilon;
    });

    registry.RegisterThen("the final price after discount should be {float}", [](TestContext& ctx, double expected) -> bool {
        double final_price = ctx.Get<double>("final_price");
        const double epsilon = 0.01;
        return std::abs(final_price - expected) < epsilon;
    });

    registry.RegisterAnd("the final price after discount should be {float}", [](TestContext& ctx, double expected) -> bool {
        double final_price = ctx.Get<double>("final_price");
        const double epsilon = 0.01;
        return std::abs(final_price - expected) < epsilon;
    });

    registry.RegisterBut("the order should have {int} items",
        [](TestContext& ctx, int unexpectedCount) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        return static_cast<int>(items.size()) != unexpectedCount;
    });

    // ---- Before hook: log for @premium tag ----
    registry.AddBeforeHook(
        {"premium"},
        [](TestContext& ctx) {
            std::cout << "  [Before] Premium order processing started\n";
            ctx.Set("premium_flag", true);
        }
    );

    // ---- After hook: cleanup for all scenarios ----
    registry.AddAfterHook(
        {},
        [](TestContext& ctx) {
            std::cout << "  [After] Order processing completed\n";
        }
    );

    const std::string_view feature = R"feature(
Feature: Order processing with discounts and validation
  Background:
    Given an empty order

  Scenario: Simple order without discount
    When I add a widget for 10.5
    And I add a gadget for 20.0
    Then the order should contain 2 items
    And the order total should be 30.5

  @premium
  Scenario: Premium order with discount
    When I add a premium-widget for 50.0
    And I add a premium-service for 25.0
    And I apply a 10 percent discount
    Then the order should contain 2 items
    And the final price after discount should be 67.5
    But the order should have 1 items

  @standard
  Scenario: Standard order with multiple items
    When I add a item-a for 5.99
    And I add a item-b for 3.50
    And I add a item-c for 1.25
    Then the order should contain 3 items
    And the order total should be 10.74
)feature";

    const auto result = RunFeature(feature, registry, "examples/GherkinAdvanced.cpp");
    return result.allPassed ? 0 : 1;
}
