#ifndef BABYBEHAVE_EXAMPLES_GHERKIN_LIBRARY_STEPS_HPP
#define BABYBEHAVE_EXAMPLES_GHERKIN_LIBRARY_STEPS_HPP

#include <BabyBehave/bdd.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// Shared step-definition "library" for a small library's book lending
// system: checkout/return, a first-come-first-served hold queue, and
// overdue fines. Included by every GherkinLibrary*.cpp example under
// examples/gherkin/ - each builds its own StepRegistry from
// MakeLibraryStepRegistry() and exercises a different, realistic scenario
// against the same ~13 shared step definitions. Companion to
// BakerySteps.hpp; see that header's comment for the general rationale
// (small RegisterLibrary*Steps(registry) functions per sub-domain,
// composed by MakeLibraryStepRegistry() below).
//
// Example-only code, not part of the installed library - the
// `using namespace` directives below are intentionally scoped to this
// header's small, self-contained audience (files under examples/gherkin/).

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

namespace library_steps {

inline constexpr int kLoanPeriodDays = 14;
inline constexpr double kFinePerOverdueDay = 0.25;
inline constexpr double kEpsilon = 0.01;

// Typed context keys - see BakerySteps.hpp's comment on Key<T> for why.
inline constexpr Key<std::string> kBookTitle{"book_title"};
inline constexpr Key<bool> kAvailable{"available"};
inline constexpr Key<std::string> kCheckedOutBy{"checked_out_by"};
inline constexpr Key<std::vector<std::string>> kHolds{"holds"};
inline constexpr Key<std::unordered_map<std::string, double>> kFines{"fines"};

// ---- Setup: one book, any number of members ----

inline bool GivenLibraryHasCopyOf(TestContext& ctx, std::string title) {
    ctx.Set(kBookTitle, std::move(title));
    ctx.Set(kAvailable, true);
    ctx.Set(kCheckedOutBy, std::string());
    ctx.Set(kHolds, std::vector<std::string>{});
    ctx.Set(kFines, std::unordered_map<std::string, double>{});
    return true;
}

inline bool GivenMemberInGoodStanding(TestContext& ctx, std::string name) {
    ctx.Mutate(kFines)[name] = 0.0;
    return true;
}

inline void RegisterLibrarySetupSteps(StepRegistry& registry) {
    registry.RegisterGiven("the library has a copy of {string}", GivenLibraryHasCopyOf);
    registry.RegisterStep({Keyword::Given, Keyword::And}, "{word} is a library member in good standing",
        GivenMemberInGoodStanding);
}

// ---- Checkout / hold / return ----

inline bool WhenChecksOutBook(TestContext& ctx, std::string member, std::string title) {
    if (!ctx.Get(kAvailable) || ctx.Get(kBookTitle) != title) {
        return false;
    }
    ctx.Set(kAvailable, false);
    ctx.Set(kCheckedOutBy, std::move(member));
    return true;
}

inline bool WhenPlacesHoldOnBook(TestContext& ctx, std::string member, std::string title) {
    if (ctx.Get(kBookTitle) != title) {
        return false;
    }
    ctx.Mutate(kHolds).push_back(std::move(member));
    return true;
}

inline bool WhenReturnsBookAfterDays(TestContext& ctx, std::string member, int daysHeld) {
    ctx.Set(kAvailable, true);
    ctx.Set(kCheckedOutBy, std::string());
    const int overdueDays = daysHeld - kLoanPeriodDays;
    if (overdueDays > 0) {
        ctx.Mutate(kFines)[member] += overdueDays * kFinePerOverdueDay;
    }
    return true;
}

inline bool WhenHoldForMemberIsFulfilled(TestContext& ctx, std::string member) {
    auto& holds = ctx.Mutate(kHolds);
    const auto it = std::find(holds.begin(), holds.end(), member);
    if (it == holds.end()) {
        return false;
    }
    holds.erase(it);
    ctx.Set(kAvailable, false);
    ctx.Set(kCheckedOutBy, std::move(member));
    return true;
}

inline void RegisterLibraryCirculationSteps(StepRegistry& registry) {
    registry.RegisterStep({Keyword::When, Keyword::And}, "{word} checks out {string}", WhenChecksOutBook);
    registry.RegisterStep({Keyword::When, Keyword::And}, "{word} places a hold on {string}", WhenPlacesHoldOnBook);
    registry.RegisterStep({Keyword::When, Keyword::And}, "{word} returns the book after {int} days",
        WhenReturnsBookAfterDays);
    registry.RegisterStep({Keyword::When, Keyword::And},
        "the hold for {word} is fulfilled and the book is checked out to them", WhenHoldForMemberIsFulfilled);
}

// ---- Availability assertions ----

inline bool ThenBookShouldBeAvailable(TestContext& ctx) {
    return ctx.Get(kAvailable);
}

inline bool ThenBookShouldBeCheckedOutTo(TestContext& ctx, std::string member) {
    return ctx.Get(kCheckedOutBy) == member;
}

inline void RegisterLibraryAvailabilityAssertionSteps(StepRegistry& registry) {
    registry.RegisterStep({Keyword::Then, Keyword::And}, "the book should be available", ThenBookShouldBeAvailable);
    registry.RegisterThen("the book should be checked out to {word}", ThenBookShouldBeCheckedOutTo);
}

// ---- Fine and hold-queue assertions ----

inline bool ThenMemberShouldOweFine(TestContext& ctx, std::string member, double expected) {
    return std::abs(ctx.Get(kFines)[member] - expected) < kEpsilon;
}

inline bool MemberShouldNotOweAnyFine(TestContext& ctx, std::string member) {
    return std::abs(ctx.Get(kFines)[member]) < kEpsilon;
}

inline bool ThenHoldQueueShouldContain(TestContext& ctx, int expected) {
    return static_cast<int>(ctx.Get(kHolds).size()) == expected;
}

inline bool AndMemberAtFrontOfHoldQueue(TestContext& ctx, std::string member) {
    const auto holds = ctx.Get(kHolds);
    return !holds.empty() && holds.front() == member;
}

inline void RegisterLibraryFineAndHoldQueueAssertionSteps(StepRegistry& registry) {
    registry.RegisterThen("{word} should owe a fine of {float}", ThenMemberShouldOweFine);
    registry.RegisterStep({Keyword::And, Keyword::But}, "{word} should not owe any fine", MemberShouldNotOweAnyFine);
    registry.RegisterThen("the hold queue should contain {int} member", ThenHoldQueueShouldContain);
    registry.RegisterAnd("{word} should be at the front of the hold queue", AndMemberAtFrontOfHoldQueue);
}

} // namespace library_steps

inline StepRegistry MakeLibraryStepRegistry() {
    StepRegistry registry;
    library_steps::RegisterLibrarySetupSteps(registry);
    library_steps::RegisterLibraryCirculationSteps(registry);
    library_steps::RegisterLibraryAvailabilityAssertionSteps(registry);
    library_steps::RegisterLibraryFineAndHoldQueueAssertionSteps(registry);
    return registry;
}

#endif // BABYBEHAVE_EXAMPLES_GHERKIN_LIBRARY_STEPS_HPP
