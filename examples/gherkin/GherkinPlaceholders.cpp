#include <BabyBehave/bdd.hpp>
#include <cmath>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    // Demonstrate different placeholder types: {int}, {float}, {string}, {word}

    auto setItemCountFn = [](TestContext& ctx, int count) -> bool {
        ctx.Set("item_count", count);
        return true;
    };

    auto setPriceFn = [](TestContext& ctx, double price) -> bool {
        ctx.Set("price", price);
        return true;
    };

    auto setProductNameFn = [](TestContext& ctx, std::string name) -> bool {
        ctx.Set("product_name", name);
        return true;
    };

    registry.RegisterGiven("I have {int} items", setItemCountFn);
    registry.RegisterGiven("a price of {float}", setPriceFn);
    registry.RegisterGiven("a product named {string}", setProductNameFn);

    // Also register for And since Gherkin uses And after Given
    registry.RegisterAnd("I have {int} items", setItemCountFn);
    registry.RegisterAnd("a price of {float}", setPriceFn);
    registry.RegisterAnd("a product named {string}", setProductNameFn);

    registry.RegisterWhen("I calculate total cost", [](TestContext& ctx) -> bool {
        int count = ctx.Get<int>("item_count");
        double price = ctx.Get<double>("price");
        double total = count * price;
        ctx.Set("total_cost", total);
        return true;
    });

    registry.RegisterThen("total should be approximately {float}", [](TestContext& ctx, double expected) -> bool {
        double total = ctx.Get<double>("total_cost");
        // Use epsilon comparison for floating point
        const double epsilon = 0.01;
        return std::fabs(total - expected) < epsilon;
    });

    registry.RegisterThen("product is {word}", [](TestContext& ctx, std::string status) -> bool {
        // Just verify the word was captured
        return !status.empty();
    });

    const std::string_view feature = R"feature(
Feature: Different placeholder types
  Scenario: Integer placeholder
    Given I have 10 items
    Then product is available

  Scenario: Float and calculation
    Given I have 5 items
    And a price of 2.5
    When I calculate total cost
    Then total should be approximately 12.5

  Scenario: String placeholder for names
    Given a product named "Premium Widget"
    Then product is available
)feature";

    const auto result = RunFeature(feature, registry, "examples/GherkinPlaceholders.cpp");
    return result.allPassed ? 0 : 1;
}
