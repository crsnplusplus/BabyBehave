#include "LibrarySteps.hpp"
#include "LoadFeatureFile.hpp"

// Hold queue / reservations flow: a second member places a hold on a book
// that's already checked out, then the hold is fulfilled once the book
// comes back. This test also demonstrates StepRegistry::Merge() by adding
// one extra, file-specific step definition (a pickup notification) on top
// of the shared library registry, without touching LibrarySteps.hpp itself.
//
// The scenario text lives in examples/gherkin/features/
// library_holds_and_reservations.feature, read from disk via
// LoadFeatureFile() - see GherkinBakeryStandardOrder.cpp's comment for why.

namespace {

// One-line body: a genuinely trivial lambda, left inline rather than
// promoted to a named function (see the project's naming convention for
// when a step earns a name).
bool AndLibraryNotifiesMemberBookIsReadyForPickup(TestContext& ctx, std::string member, std::string title) {
    return ctx.Get(library_steps::kCheckedOutBy) == member && ctx.Get(library_steps::kBookTitle) == title;
}

} // namespace

StepRegistry PrepareRegistry() {
    StepRegistry registry = MakeLibraryStepRegistry();

    // Extra step definition specific to this file: a pickup notification
    // that only this hold-fulfillment scenario cares about, layered onto
    // the shared registry via Merge() instead of every Library example
    // paying for it.
    StepRegistry notificationExtras;
    notificationExtras.RegisterAnd(
        "the library notifies {word} that {string} is ready for pickup", AndLibraryNotifiesMemberBookIsReadyForPickup);
    registry.Merge(notificationExtras);

    return registry;
}

int main() {
    StepRegistry registry = PrepareRegistry();
    return RunFeatureFromFile(registry, "library_holds_and_reservations.feature");
}
