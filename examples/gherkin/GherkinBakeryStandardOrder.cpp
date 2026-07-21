#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

// Standard order fulfillment: a plain cake, paid off in full, no allergen
// substitutions, no cancellation. This is the "happy path" test built
// straight from the shared registry, with no Merge() and no extra,
// file-specific steps - see GherkinBakeryAllergenSubstitution.cpp for a
// scenario that does add its own steps via Merge().
//
// The scenario text itself lives in a real, standalone .feature file
// (examples/gherkin/features/bakery_standard_order.feature), read from
// disk via LoadFeatureFile() below rather than embedded as a C++ raw
// string literal - RunFeature() is unaware of the difference either way,
// since it only ever sees the resulting std::string_view.

int main() {
    StepRegistry registry = MakeBakeryStepRegistry();

    const std::string feature = LoadFeatureFile("bakery_standard_order.feature");

    const auto result = RunFeature(feature, registry, "examples/gherkin/features/bakery_standard_order.feature");
    return result.allPassed ? 0 : 1;
}
