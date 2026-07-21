#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

#include <future>
#include <iostream>
#include <mutex>
#include <vector>

// Demonstrates the "one shared registry, fanned out across many threads"
// pattern: MakeLibraryStepRegistry() is built exactly ONCE up front, then
// four branches (Uptown/Downtown/Riverside/Harbor) each run their own,
// different .feature file concurrently against that SAME StepRegistry
// instance - contrast with GherkinMultiThreaded.cpp, where every thread
// builds its OWN, independent registry instead.
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
// Safety note (mirrors a risk already flagged for v0.9.0's planned
// parallel-execution feature): RunFeature()'s DEFAULT onFailure callback
// prints and calls std::exit() and is NOT safe to invoke concurrently from
// several scenario threads at once - so this example passes an explicit,
// mutex-guarded callback that only ever appends to a shared vector, never
// exits, and each thread's FeatureResult is collected and inspected back
// on the main thread after every future has been joined.

namespace {

StepRegistry PrepareRegistry() {
    return MakeLibraryStepRegistry();
}

FeatureResult RunBranchScenario(StepRegistry& registry, const GherkinFailureCallback& onFailure, int branchId) {
    const std::string featureFile = "concurrent_branch_" + std::to_string(branchId) + ".feature";
    const std::string featureText = LoadFeatureFile(featureFile);
    return RunFeature(featureText, registry, featureFile, onFailure);
}

} // namespace

int main() {
    // Built once; every thread below only reads from it.
    StepRegistry registry = PrepareRegistry();

    // Mutex-guarded, non-exiting: safe to invoke from any of the branch
    // threads, unlike the default onFailure callback (see comment above).
    std::mutex failuresMutex;
    std::vector<std::string> collectedFailures;
    const GherkinFailureCallback threadSafeCollectFailures = [&](std::string_view message) {
        std::lock_guard<std::mutex> lock(failuresMutex);
        collectedFailures.emplace_back(message);
    };

    constexpr int kBranchCount = 4;
    std::vector<std::future<FeatureResult>> futures;
    futures.reserve(kBranchCount);
    for (int branchId = 0; branchId < kBranchCount; ++branchId) {
        futures.push_back(std::async(std::launch::async, [&registry, &threadSafeCollectFailures, branchId] {
            return RunBranchScenario(registry, threadSafeCollectFailures, branchId);
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
