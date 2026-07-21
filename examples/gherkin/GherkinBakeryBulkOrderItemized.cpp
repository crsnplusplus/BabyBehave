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

StepRegistry PrepareRegistry() {
    StepRegistry registry;

    constexpr double kEpsilon = 0.01;

    // ---- Order setup ----
    registry.RegisterGiven("a bulk order for a wedding ceremony", [](TestContext& ctx) -> bool {
        ctx.Set("order_total", 0.0);
        ctx.Set("line_items", std::vector<std::string>{});
        return true;
    });

    // ---- Data table parsing and total calculation ----
    registry.RegisterWhen("the order contains the following items:", 
        [](TestContext& ctx, const DataTable& table) -> bool {
            double total = 0.0;
            std::vector<std::string> items;

            // Iterate over data rows (exclude header at rows[0])
            for (std::size_t i = 0; i < table.RowCount(); ++i) {
                // Get values by column name using the header-aware Get() method
                std::string item_name = table.Get(i, "item");
                std::string quantity_str = table.Get(i, "quantity");
                std::string unit_price_str = table.Get(i, "unit_price");

                // Parse quantity and unit price
                int quantity = std::stoi(quantity_str);
                double unit_price = std::stod(unit_price_str);

                // Accumulate total and track items
                total += quantity * unit_price;
                items.push_back(item_name);
            }

            ctx.Set("order_total", total);
            ctx.Set("line_items", items);
            return true;
        });

    // ---- Assertions ----
    registry.RegisterThen("the order total should be {float}", 
        [](TestContext& ctx, double expected) -> bool {
            double actual = ctx.Get<double>("order_total");
            return std::abs(actual - expected) < kEpsilon;
        });

    registry.RegisterAnd("the order should have {int} line items",
        [](TestContext& ctx, int expected) -> bool {
            auto items = ctx.Get<std::vector<std::string>>("line_items");
            return static_cast<int>(items.size()) == expected;
        });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "bakery_bulk_order_itemized.feature");
}
