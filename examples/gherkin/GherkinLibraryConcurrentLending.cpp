#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

#include <future>
#include <iostream>
#include <vector>

// Demonstrates the "one shared registry, fanned out across many threads"
// pattern: MakeLibraryStepRegistry() is built exactly ONCE up front, then
// four branches (Uptown/Downtown/Riverside/Harbor) each run their own,
// different .feature file concurrently against that SAME StepRegistry
// instance - contrast with GherkinMultiThreaded.cpp, where every thread
// builds its OWN, independent registry instead. See
// GherkinBakerySequentialFeatures.cpp for a SEQUENTIAL (non-threaded)
// counterpart to this same "one registry, several .feature files" idea.
//
// Why this is safe: registration (RegisterGiven/.../Merge()) happens only
// on the main thread, entirely before any thread is launched. From that
// point on, RunFeature() only ever calls StepRegistry's const member
// functions (TryMatch(), BeforeHooks(), AfterHooks()) - concurrent const
// reads of an object nobody is mutating are safe, the same guarantee the
// standard library gives std::vector/std::string readers. Each branch's
// scenario still gets its own private TestContext internally (RunScenario
// constructs a fresh BabyBehaveTest per Scenario), so there is no shared
// mutable state between branches despite the shared registry.
//
// Safety note: the DEFAULT onFailure callback prints and calls std::exit()
// and is NOT safe to invoke concurrently from several scenario threads at
// once - so this example passes Gherkin::CollectingFailureHandler, a
// thread-safe, non-exiting callback that only ever appends to a shared
// vector, and each thread's FeatureResult is collected and inspected back
// on the main thread after every future has been joined.

namespace {

StepRegistry PrepareRegistry() {
    return MakeLibraryStepRegistry();
}

FeatureResult RunBranchScenario(StepRegistry& registry, const GherkinFailureCallback& onFailure, int branchId) {
    const std::string featureFile = "concurrent_branch_" + std::to_string(branchId) + ".feature";
    std::string featureText = LoadFeatureFile(featureFile);
    return Feature(std::move(featureText), registry).Label(featureFile).OnFailure(onFailure).Run();
}

} // namespace

int main() {
    // Built once; every thread below only reads from it.
    StepRegistry registry = PrepareRegistry();

    // Thread-safe, non-exiting: safe to invoke from any of the branch
    // threads, unlike the default onFailure callback (see comment above).
    std::vector<std::string> collectedFailures;
    const CollectingFailureHandler collectFailures(collectedFailures);

    constexpr int kBranchCount = 4;
    std::vector<std::future<FeatureResult>> futures;
    futures.reserve(kBranchCount);
    for (int branchId = 0; branchId < kBranchCount; ++branchId) {
        futures.push_back(std::async(std::launch::async, [&registry, &collectFailures, branchId] {
            return RunBranchScenario(registry, collectFailures, branchId);
        }));
    }

    bool allPassed = true;
    for (auto& future : futures) {
        const FeatureResult result = future.get();
        allPassed = allPassed && result.allPassed;
    }

    std::cout << kBranchCount << " branches ran concurrently against one shared StepRegistry.\n";
    for (const auto& message : collectedFailures) {
        std::cerr << "  - " << message << '\n';
    }

    return allPassed ? 0 : 1;
}
