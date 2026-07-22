#include <BabyBehave/bdd.hpp>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Advanced Gherkin example combining:
// - Background: common setup shared across scenarios
// - Multiple Scenario with @tags
// - Before/After hooks scoped to tags
// - Multiple placeholder types: {int}, {string}, {float}, {word}
// - And/But step connectors
// - Realistic domain: order processing with validation and pricing

namespace {

constexpr double kEpsilon = 0.01;

constexpr Key<std::vector<std::string>> kItems{"items"};
constexpr Key<double> kTotalPrice{"total_price"};
constexpr Key<double> kDiscount{"discount"};
constexpr Key<double> kFinalPrice{"final_price"};

// ---- Background: Common order setup ----
bool GivenEmptyOrder(TestContext& ctx) {
    ctx.Set(kItems, std::vector<std::string>{});
    ctx.Set(kTotalPrice, 0.0);
    ctx.Set(kDiscount, 0.0);
    return true;
}

// ---- Item addition (When/And) ----
bool WhenAddItemForPrice(TestContext& ctx, std::string item, double price) {
    ctx.Mutate(kItems).push_back(item + " ($" + std::to_string(price) + ")");
    ctx.Mutate(kTotalPrice) += price;
    return true;
}

// ---- Discount application ----
bool WhenApplyPercentDiscount(TestContext& ctx, int discountPercent) {
    const double total = ctx.Get(kTotalPrice);
    const double discountAmount = (total * discountPercent) / 100.0;
    ctx.Set(kDiscount, discountAmount);
    ctx.Set(kFinalPrice, total - discountAmount);
    return true;
}

// ---- Validation (Then/And/But) ----
bool ThenOrderShouldContainItems(TestContext& ctx, int expectedCount) {
    return static_cast<int>(ctx.Get(kItems).size()) == expectedCount;
}

bool ThenOrderTotalShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kTotalPrice) - expected) < kEpsilon;
}

bool ThenFinalPriceAfterDiscountShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kFinalPrice) - expected) < kEpsilon;
}

bool ButOrderShouldHaveItems(TestContext& ctx, int unexpectedCount) {
    return static_cast<int>(ctx.Get(kItems).size()) != unexpectedCount;
}

// ---- Before hook: log for @premium tag ----
void LogPremiumOrderStart(TestContext& ctx) {
    std::cout << "  [Before] Premium order processing started\n";
    ctx.Set(Key<bool>{"premium_flag"}, true);
}

// ---- After hook: cleanup for all scenarios ----
void LogOrderProcessingComplete(TestContext& ctx) {
    std::cout << "  [After] Order processing completed\n";
}

} // namespace

int main() {
    StepRegistry registry;

    registry.RegisterGiven("an empty order", GivenEmptyOrder);
    registry.RegisterStep({Keyword::When, Keyword::And}, "I add a {word} for {float}", WhenAddItemForPrice);
    registry.RegisterStep({Keyword::When, Keyword::And}, "I apply a {int} percent discount", WhenApplyPercentDiscount);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the order should contain {int} items", ThenOrderShouldContainItems);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the order total should be {float}", ThenOrderTotalShouldBe);
    registry.RegisterStep(
        {Keyword::Then, Keyword::And}, "the final price after discount should be {float}", ThenFinalPriceAfterDiscountShouldBe);
    registry.RegisterBut("the order should have {int} items", ButOrderShouldHaveItems);

    // Before/After hooks below use DIFFERENT tag filters ({"premium"} vs
    // the empty/always-run filter) - AddAroundHook only applies when a
    // Before+After pair shares one tag filter, so these stay as two
    // separate registrations, each still demonstrating its own concept
    // (tag-scoped setup vs. unconditional cleanup).
    registry.AddBeforeHook({"premium"}, LogPremiumOrderStart);
    registry.AddAfterHook({}, LogOrderProcessingComplete);

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

    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinAdvanced.cpp").Run();
    return result.ExitCode();
}
