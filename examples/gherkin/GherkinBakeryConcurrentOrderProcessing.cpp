#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

#include <iostream>
#include <mutex>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Demonstrates the NEW v0.9.0 enableParallelScenarios=true feature:
// one RunFeature() call on ONE feature file containing MULTIPLE independent
// scenarios, internally parallelized by passing enableParallelScenarios=true.
//
// This is distinct from GherkinMultiThreaded.cpp and GherkinLibraryConcurrentLending.cpp:
// - GherkinMultiThreaded.cpp: multiple separate RunFeature() calls (each with its own
//   StepRegistry, own feature text, own thread), orchestrated by the CONSUMER via std::async.
// - GherkinLibraryConcurrentLending.cpp: one shared StepRegistry, multiple separate
//   RunFeature() calls on different .feature files, each on its own thread, orchestrated
//   by the CONSUMER via std::async.
// - THIS EXAMPLE: one RunFeature() call on ONE feature file containing multiple independent
//   scenarios, internally parallelized by the LIBRARY when you pass enableParallelScenarios=true.
//
// Safety precondition (same as GherkinLibraryConcurrentLending.cpp, which this pattern
// mirrors): each scenario must have zero shared mutable state with others. Our bakery
// order scenarios satisfy this because every Scenario gets its own fresh TestContext
// (and all its variables) - no shared inventory, no shared payment state, nothing that
// could race. The StepRegistry is shared and read-only (no mutations after RunFeature
// starts), so concurrent reads are safe.
//
// CRITICAL: the DEFAULT onFailure callback (which calls std::exit()) is NOT SAFE under
// parallel mode. We supply an explicit, mutex-guarded callback that only appends to a
// shared vector and never exits or throws - safe to invoke concurrently from scenario
// threads without hazard.
//
// The feature text lives in examples/gherkin/features/bakery_concurrent_order_processing.feature.

StepRegistry PrepareRegistry() {
    return MakeBakeryStepRegistry();
}

int main() {
    // Load feature file
    const std::string featureText = LoadFeatureFile("bakery_concurrent_order_processing.feature");
    const std::string featureLabel = "examples/gherkin/features/bakery_concurrent_order_processing.feature";

    // Prepare registry (built once; shared across all scenarios, but only read from)
    StepRegistry registry = PrepareRegistry();

    // Mutex-guarded, non-exiting failure collection: REQUIRED for parallel mode.
    // Unlike the default onFailure (which calls std::exit()), this is safe to invoke
    // concurrently from multiple scenario threads.
    std::mutex failuresMutex;
    std::vector<std::string> collectedFailures;
    const GherkinFailureCallback threadSafeCollectFailures = [&](std::string_view message) {
        std::lock_guard<std::mutex> lock(failuresMutex);
        collectedFailures.emplace_back(message);
    };

    // Run the feature with internal parallelism: each scenario is dispatched to its own
    // std::async(std::launch::async, ...) task. Results are always collected in original
    // declaration order (no sorting needed).
    const auto result = RunFeature(
        featureText,
        registry,
        featureLabel,
        threadSafeCollectFailures,
        true  // enableParallelScenarios = true
    );

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

    // The consumer (us) decides the process's fate, not RunFeature() itself.
    // We mirror the library's own convention: exit with 0 on all-passed, 1 otherwise.
    return result.allPassed ? 0 : 1;
}
