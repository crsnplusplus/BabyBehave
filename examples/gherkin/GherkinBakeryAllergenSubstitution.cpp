#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

// Order with an allergen substitution: the shared registry already prices
// the surcharge in, but this test also demonstrates StepRegistry::Merge()
// by adding one extra, file-specific step definition on top of the shared
// bakery library - a kitchen-certification check that only this scenario
// needs, without touching BakerySteps.hpp itself.
//
// The scenario text lives in examples/gherkin/features/
// bakery_allergen_substitution.feature, read from disk via
// LoadFeatureFile() - see GherkinBakeryStandardOrder.cpp's comment for why.

StepRegistry PrepareRegistry() {
    StepRegistry registry = MakeBakeryStepRegistry();

    // Extra step definition specific to this file: confirms the kitchen's
    // allergen-handling note, layered onto the shared registry via Merge()
    // rather than baked into BakerySteps.hpp (which every other Bakery
    // example also includes, and doesn't need this check).
    StepRegistry kitchenExtras;
    kitchenExtras.RegisterAnd("the kitchen confirms the substitution is {word}",
        [](TestContext& ctx, std::string note) -> bool {
            return note == "nut-free" && !ctx.Get<std::vector<std::string>>("allergens").empty();
        });
    registry.Merge(kitchenExtras);

    return registry;
}

int main() {
    auto registry = PrepareRegistry();
    return RunFeatureFromFile(registry, "bakery_allergen_substitution.feature");
}
