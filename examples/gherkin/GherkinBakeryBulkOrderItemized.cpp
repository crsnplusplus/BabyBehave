#include "LoadFeatureFile.hpp"
#include <BabyBehave/bdd.hpp>
#include <cmath>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Data Tables example: a bulk order scenario where a step is followed by
// a pipe-delimited table of line items (quantity and unit price per item).
// The step definition receives the table via a trailing `const DataTable&`
// parameter and iterates over the rows to compute the order total.
//
// The scenario text lives in examples/gherkin/features/
// bakery_bulk_order_itemized.feature, read from disk via LoadFeatureFile().

namespace {

constexpr double kEpsilon = 0.01;

// Typed context keys - see BakerySteps.hpp's comment on Key<T> for why.
constexpr Key<double> kOrderTotal{"order_total"};
constexpr Key<std::vector<std::string>> kLineItems{"line_items"};

bool GivenBulkOrderForWeddingCeremony(TestContext& ctx) {
    ctx.Set(kOrderTotal, 0.0);
    ctx.Set(kLineItems, std::vector<std::string>{});
    return true;
}

// ---- Data table parsing and total calculation ----
bool WhenOrderContainsFollowingItems(TestContext& ctx, const DataTable& table) {
    double total = 0.0;
    std::vector<std::string> items;

    // Iterate over data rows (exclude header at rows[0])
    for (std::size_t i = 0; i < table.RowCount(); ++i) {
        // Get values by column name using the header-aware Get() method
        const std::string itemName = table.Get(i, "item");
        const int quantity = std::stoi(table.Get(i, "quantity"));
        const double unitPrice = std::stod(table.Get(i, "unit_price"));

        // Accumulate total and track items
        total += quantity * unitPrice;
        items.push_back(itemName);
    }

    ctx.Set(kOrderTotal, total);
    ctx.Set(kLineItems, std::move(items));
    return true;
}

bool ThenOrderTotalShouldBe(TestContext& ctx, double expected) {
    return std::abs(ctx.Get(kOrderTotal) - expected) < kEpsilon;
}

bool AndOrderShouldHaveLineItems(TestContext& ctx, int expected) {
    return static_cast<int>(ctx.Get(kLineItems).size()) == expected;
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry;
    registry.RegisterSteps(
        StepEntry{Keyword::Given, "a bulk order for a wedding ceremony", GivenBulkOrderForWeddingCeremony},
        StepEntry{Keyword::When, "the order contains the following items:", WhenOrderContainsFollowingItems},
        StepEntry{Keyword::Then, "the order total should be {float}", ThenOrderTotalShouldBe},
        StepEntry{Keyword::And, "the order should have {int} line items", AndOrderShouldHaveLineItems});
    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_bulk_order_itemized.feature");
}
