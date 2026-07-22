#include <BabyBehave/bdd.hpp>
#include <cmath>
#include <string>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Demonstrate different placeholder types: {int}, {float}, {string}, {word}

namespace {

constexpr double kEpsilon = 0.01;

constexpr Key<int> kItemCount{"item_count"};
constexpr Key<double> kPrice{"price"};
constexpr Key<std::string> kProductName{"product_name"};
constexpr Key<double> kTotalCost{"total_cost"};

bool GivenItemCount(TestContext& ctx, int count) {
    ctx.Set(kItemCount, count);
    return true;
}

bool GivenPriceOf(TestContext& ctx, double price) {
    ctx.Set(kPrice, price);
    return true;
}

bool GivenProductNamed(TestContext& ctx, std::string name) {
    ctx.Set(kProductName, std::move(name));
    return true;
}

bool WhenCalculateTotalCost(TestContext& ctx) {
    ctx.Set(kTotalCost, ctx.Get(kItemCount) * ctx.Get(kPrice));
    return true;
}

bool ThenTotalShouldBeApproximately(TestContext& ctx, double expected) {
    return std::fabs(ctx.Get(kTotalCost) - expected) < kEpsilon;
}

bool ThenProductIs(TestContext& ctx, std::string status) {
    // Just verify the word was captured
    return !status.empty();
}

} // namespace

int main() {
    StepRegistry registry;

    // Also register for And since Gherkin uses And after Given
    registry.RegisterStep({Keyword::Given, Keyword::And}, "I have {int} items", GivenItemCount);
    registry.RegisterStep({Keyword::Given, Keyword::And}, "a price of {float}", GivenPriceOf);
    registry.RegisterStep({Keyword::Given, Keyword::And}, "a product named {string}", GivenProductNamed);
    registry.RegisterWhen("I calculate total cost", WhenCalculateTotalCost);
    registry.RegisterThen("total should be approximately {float}", ThenTotalShouldBeApproximately);
    registry.RegisterThen("product is {word}", ThenProductIs);

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

    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinPlaceholders.cpp").Run();
    return result.ExitCode();
}
