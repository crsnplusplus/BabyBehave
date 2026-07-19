#include <BabyBehave/bdd.hpp>
#include <algorithm>
#include <iostream>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

int main() {
    StepRegistry registry;

    // Register Given steps
    registry.RegisterGiven("an empty basket", [](TestContext& ctx) -> bool {
        ctx.Set("items", std::vector<std::string>{});
        return true;
    });

    // Helper lambdas to add items
    auto addItemsFn = [](TestContext& ctx, int count, std::string itemType) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        for (int i = 0; i < count; ++i) {
            items.push_back(itemType);
        }
        ctx.Set("items", items);
        return true;
    };

    auto checkItemsFn = [](TestContext& ctx, int expectedCount) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        return items.size() == static_cast<std::size_t>(expectedCount);
    };

    auto containsItemFn = [](TestContext& ctx, int count, std::string itemType) -> bool {
        auto items = ctx.Get<std::vector<std::string>>("items");
        const auto actualCount = std::count(items.begin(), items.end(), itemType);
        return actualCount == count;
    };

    // Register When steps
    registry.RegisterWhen("I add {int} {word}", addItemsFn);

    // Register And steps (needed because "And" is a separate keyword category)
    registry.RegisterAnd("I add {int} {word}", addItemsFn);
    registry.RegisterAnd("the basket has {int} items", checkItemsFn);
    registry.RegisterAnd("the basket contains {int} {word}", containsItemFn);

    // Register Then steps
    registry.RegisterThen("the basket has {int} items", checkItemsFn);
    registry.RegisterThen("the basket contains {int} {word}", containsItemFn);

    const std::string_view feature = R"feature(
Feature: Shopping basket
  Scenario: Add multiple items to a basket
    Given an empty basket
    When I add 3 apple(s)
    And I add 2 orange(s)
    Then the basket has 5 items
    And the basket contains 3 apple(s)
    And the basket contains 2 orange(s)
)feature";

    const auto result = RunFeature(feature, registry, "examples/GherkinBasket.cpp");
    return result.allPassed ? 0 : 1;
}
