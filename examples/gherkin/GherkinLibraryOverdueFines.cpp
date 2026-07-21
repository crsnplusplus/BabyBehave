#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

// Overdue fine calculation: a book returned five days late accrues a fine
// at the shared registry's per-day rate, exercising the same step
// definitions as GherkinLibraryStandardLending.cpp but with a different,
// fine-triggering scenario - no Merge() needed here, just a different
// .feature file against the identical registry.
//
// The scenario text lives in examples/gherkin/features/
// library_overdue_fines.feature, read from disk via LoadFeatureFile()
// - see GherkinBakeryStandardOrder.cpp's comment for why.

StepRegistry PrepareRegistry() {
    return MakeLibraryStepRegistry();
}

int main() {
    StepRegistry registry = PrepareRegistry();
    return RunFeatureFromFile(registry, "library_overdue_fines.feature");
}
