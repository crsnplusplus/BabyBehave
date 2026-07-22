#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

#include <iostream>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Demonstrates the v0.9.0 enableParallelScenarios=true feature via the
// Feature(...).Parallel(true) fluent builder: one Run() call on ONE feature
// file containing MULTIPLE independent scenarios, internally parallelized
// by the library.
//
// This is distinct from GherkinMultiThreaded.cpp and GherkinLibraryConcurrentLending.cpp:
// - GherkinMultiThreaded.cpp: multiple separate Feature(...).Run() calls (each with its own
//   StepRegistry, own feature text, own thread), orchestrated by the CONSUMER via std::async.
// - GherkinLibraryConcurrentLending.cpp: one shared StepRegistry, multiple separate
//   Feature(...).Run() calls on different .feature files, each on its own thread, orchestrated
//   by the CONSUMER via std::async.
// - THIS EXAMPLE: one Run() call on ONE feature file containing multiple independent
//   scenarios, internally parallelized by the LIBRARY via .Parallel(true).
//
// Safety precondition (same as GherkinLibraryConcurrentLending.cpp, which this pattern
// mirrors): each scenario must have zero shared mutable state with others. Our bakery
// order scenarios satisfy this because every Scenario gets its own fresh TestContext
// (and all its variables) - no shared inventory, no shared payment state, nothing that
// could race. The StepRegistry is shared and read-only (no mutations after the Feature
// starts running), so concurrent reads are safe.
//
// CRITICAL: the DEFAULT onFailure callback (which calls std::exit()) is NOT SAFE under
// parallel mode. We supply Gherkin::CollectingFailureHandler via .OnFailure() instead -
// a thread-safe, non-exiting callback that just appends to a shared vector, replacing
// the hand-rolled mutex+vector+lambda this example used to carry itself.
//
// The feature text lives in examples/gherkin/features/bakery_concurrent_order_processing.feature.

StepRegistry PrepareRegistry() {
    return MakeBakeryStepRegistry();
}

int main() {
    // Load feature file (kept via the example-only LoadFeatureFile.hpp
    // convention, not Gherkin::LoadFeatureFile - see the retrofit's file-
    // loading judgment call: this one resolves BABYBEHAVE_GHERKIN_FEATURES_DIR
    // so the binary finds its .feature file regardless of cwd).
    std::string featureText = ::LoadFeatureFile("bakery_concurrent_order_processing.feature");
    const std::string featureLabel = "examples/gherkin/features/bakery_concurrent_order_processing.feature";

    // Prepare registry (built once; shared across all scenarios, but only read from)
    StepRegistry registry = PrepareRegistry();

    // Thread-safe, non-exiting failure collection: REQUIRED for parallel mode.
    std::vector<std::string> collectedFailures;
    const CollectingFailureHandler collectFailures(collectedFailures);

    // Run the feature with internal parallelism: each scenario is dispatched to its own
    // std::async(std::launch::async, ...) task. Results are always collected in original
    // declaration order (no sorting needed).
    const auto result = Feature(std::move(featureText), registry)
                             .Label(featureLabel)
                             .OnFailure(collectFailures)
                             .Parallel(true)
                             .Run();

    // Narrate the results
    std::cout << "\n=== Bakery Concurrent Order Processing Results ===\n";
    std::cout << "Feature: " << result.featureName << '\n';
    std::cout << "Scenarios executed: " << result.scenarioResults.size() << '\n';
    std::cout << "All passed: " << (result.allPassed ? "YES" : "NO") << '\n';

    if (!collectedFailures.empty()) {
        std::cout << "\nFailures (" << collectedFailures.size() << "):\n";
        for (const auto& message : collectedFailures) {
            std::cout << "  - " << message << '\n';
        }
    }

    // The consumer (us) decides the process's fate, not the builder itself.
    return result.ExitCode();
}
