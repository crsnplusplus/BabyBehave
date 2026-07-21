#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

// Hold pickup deadline scenario with @timeout annotation: demonstrates how a
// Scenario tagged with @timeout:<value><unit> (unit required: s/ms/m, e.g.
// @timeout:2s) cooperatively checks elapsed time before each step starts, and
// fails that step and all subsequent ones immediately if the deadline has
// already been exceeded. This is a realistic fast-completing hold-fulfillment
// scenario (no artificial sleeps) where all steps execute well within the
// 2-second service window, demonstrating the annotation declaratively on a
// normal passing test rather than as a deliberate-failure demo.
//
// The scenario text lives in examples/gherkin/features/
// library_hold_pickup_deadline.feature, read from disk via
// LoadFeatureFile() - see GherkinBakeryStandardOrder.cpp's comment for why.

StepRegistry PrepareRegistry() {
    return MakeLibraryStepRegistry();
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "library_hold_pickup_deadline.feature");
}
