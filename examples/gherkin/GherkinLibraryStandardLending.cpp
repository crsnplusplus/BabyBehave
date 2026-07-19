#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

// Standard lending flow: check a book out and return it on time, with no
// hold queue and no fine. The "happy path" test built straight from the
// shared library registry, with no Merge() and no extra, file-specific
// steps - see GherkinLibraryHoldsAndReservations.cpp for a scenario that
// does add its own steps via Merge().
//
// The scenario text lives in examples/gherkin/features/
// library_standard_lending.feature, read from disk via LoadFeatureFile()
// - see GherkinBakeryStandardOrder.cpp's comment for why.

int main() {
    StepRegistry registry = MakeLibraryStepRegistry();

    const std::string feature = LoadFeatureFile("library_standard_lending.feature");

    const auto result = RunFeature(feature, registry, "examples/gherkin/features/library_standard_lending.feature");
    return result.allPassed ? 0 : 1;
}
