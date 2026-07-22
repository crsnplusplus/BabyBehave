#include <BabyBehave/bdd.hpp>
#include <algorithm>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace {

constexpr Key<std::vector<std::string>> kItems{"items"};

bool GivenEmptyBasket(TestContext& ctx) {
    ctx.Set(kItems, std::vector<std::string>{});
    return true;
}

bool WhenAddItems(TestContext& ctx, int count, std::string itemType) {
    auto& items = ctx.Mutate(kItems);
    for (int i = 0; i < count; ++i) {
        items.push_back(itemType);
    }
    return true;
}

bool ThenBasketHasItems(TestContext& ctx, int expectedCount) {
    return ctx.Get(kItems).size() == static_cast<std::size_t>(expectedCount);
}

bool ThenBasketContainsItems(TestContext& ctx, int count, std::string itemType) {
    const auto items = ctx.Get(kItems);
    const auto actualCount = std::count(items.begin(), items.end(), itemType);
    return actualCount == count;
}

} // namespace

int main() {
    StepRegistry registry;

    // Register step definitions as one scannable table: pattern + keyword(s) + implementation.
    registry.RegisterGiven("an empty basket", GivenEmptyBasket);
    registry.RegisterStep({Keyword::When, Keyword::And}, "I add {int} {word}", WhenAddItems);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the basket has {int} items", ThenBasketHasItems);
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the basket contains {int} {word}", ThenBasketContainsItems);

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

    const auto result = Feature(std::string(feature), registry).Label("examples/GherkinBasket.cpp").Run();
    return result.ExitCode();
}
