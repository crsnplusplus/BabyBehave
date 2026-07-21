#include "BakerySteps.hpp"
#include "LoadFeatureFile.hpp"

// Late cancellation and deposit forfeiture: this scenario deliberately
// exercises a realistic *failing* assertion, reusing the very same shared
// registry every other Bakery example uses. Cancelling only two days
// before pickup - inside the three-day grace period defined in
// BakerySteps.hpp - genuinely forfeits the deposit under the bakery's own
// policy, so a customer's expectation of a refund is simply wrong here.
// RunFeature() therefore fails hard and exits non-zero, exactly as it
// would for a real bakery test catching this business-rule violation;
// see the CI job's tolerant list in .github/workflows/ci.yml.
//
// The scenario text lives in examples/gherkin/features/
// bakery_late_cancellation.feature, read from disk via LoadFeatureFile()
// - see GherkinBakeryStandardOrder.cpp's comment for why.

StepRegistry PrepareRegistry() {
    StepRegistry registry = MakeBakeryStepRegistry();
    return registry;
}

int main() {
    // Intentionally expected to fail: "the deposit should be refunded" is
    // false whenever the deposit was forfeited, so this scenario's last
    // step fails and RunFeatureFromFile() returns 1.
    auto registry = PrepareRegistry();
    return RunFeatureFromFile(registry, "bakery_late_cancellation.feature");
}
