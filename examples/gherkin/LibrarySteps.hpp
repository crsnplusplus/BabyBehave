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
// BakerySteps.hpp; see that header's comment for the general rationale.
//
// Example-only code, not part of the installed library - the
// `using namespace` directives below are intentionally scoped to this
// header's small, self-contained audience (files under examples/gherkin/).

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

inline StepRegistry MakeLibraryStepRegistry() {
    StepRegistry registry;

    constexpr int kLoanPeriodDays = 14;
    constexpr double kFinePerOverdueDay = 0.25;
    constexpr double kEpsilon = 0.01;

    // ---- Setup: one book, any number of members ----
    registry.RegisterGiven("the library has a copy of {string}", [](TestContext& ctx, std::string title) -> bool {
        ctx.Set("book_title", title);
        ctx.Set("available", true);
        ctx.Set("checked_out_by", std::string());
        ctx.Set("holds", std::vector<std::string>{});
        ctx.Set("fines", std::unordered_map<std::string, double>{});
        return true;
    });

    auto memberInGoodStandingFn = [](TestContext& ctx, std::string name) -> bool {
        auto fines = ctx.Get<std::unordered_map<std::string, double>>("fines");
        fines[name] = 0.0;
        ctx.Set("fines", fines);
        return true;
    };
    registry.RegisterGiven("{word} is a library member in good standing", memberInGoodStandingFn);
    registry.RegisterAnd("{word} is a library member in good standing", memberInGoodStandingFn);

    // ---- Checkout / hold / return ----
    auto checksOutFn = [](TestContext& ctx, std::string member, std::string title) -> bool {
        if (!ctx.Get<bool>("available") || ctx.Get<std::string>("book_title") != title) {
            return false;
        }
        ctx.Set("available", false);
        ctx.Set("checked_out_by", member);
        return true;
    };
    registry.RegisterWhen("{word} checks out {string}", checksOutFn);
    registry.RegisterAnd("{word} checks out {string}", checksOutFn);

    auto placesHoldFn = [](TestContext& ctx, std::string member, std::string title) -> bool {
        if (ctx.Get<std::string>("book_title") != title) {
            return false;
        }
        auto holds = ctx.Get<std::vector<std::string>>("holds");
        holds.push_back(member);
        ctx.Set("holds", holds);
        return true;
    };
    registry.RegisterWhen("{word} places a hold on {string}", placesHoldFn);
    registry.RegisterAnd("{word} places a hold on {string}", placesHoldFn);

    auto returnsBookFn = [](TestContext& ctx, std::string member, int daysHeld) -> bool {
        ctx.Set("available", true);
        ctx.Set("checked_out_by", std::string());
        const int overdueDays = daysHeld - kLoanPeriodDays;
        if (overdueDays > 0) {
            auto fines = ctx.Get<std::unordered_map<std::string, double>>("fines");
            fines[member] += overdueDays * kFinePerOverdueDay;
            ctx.Set("fines", fines);
        }
        return true;
    };
    registry.RegisterWhen("{word} returns the book after {int} days", returnsBookFn);
    registry.RegisterAnd("{word} returns the book after {int} days", returnsBookFn);

    auto fulfillHoldFn = [](TestContext& ctx, std::string member) -> bool {
        auto holds = ctx.Get<std::vector<std::string>>("holds");
        const auto it = std::find(holds.begin(), holds.end(), member);
        if (it == holds.end()) {
            return false;
        }
        holds.erase(it);
        ctx.Set("holds", holds);
        ctx.Set("available", false);
        ctx.Set("checked_out_by", member);
        return true;
    };
    registry.RegisterWhen("the hold for {word} is fulfilled and the book is checked out to them", fulfillHoldFn);
    registry.RegisterAnd("the hold for {word} is fulfilled and the book is checked out to them", fulfillHoldFn);

    // ---- Assertions ----
    registry.RegisterThen("the book should be available", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("available");
    });
    registry.RegisterAnd("the book should be available", [](TestContext& ctx) -> bool {
        return ctx.Get<bool>("available");
    });

    registry.RegisterThen("the book should be checked out to {word}", [](TestContext& ctx, std::string member) -> bool {
        return ctx.Get<std::string>("checked_out_by") == member;
    });

    registry.RegisterThen("{word} should owe a fine of {float}", [](TestContext& ctx, std::string member, double expected) -> bool {
        auto fines = ctx.Get<std::unordered_map<std::string, double>>("fines");
        return std::abs(fines[member] - expected) < kEpsilon;
    });

    auto noFineOwedFn = [](TestContext& ctx, std::string member) -> bool {
        auto fines = ctx.Get<std::unordered_map<std::string, double>>("fines");
        return std::abs(fines[member]) < kEpsilon;
    };
    registry.RegisterAnd("{word} should not owe any fine", noFineOwedFn);
    registry.RegisterBut("{word} should not owe any fine", noFineOwedFn);

    registry.RegisterThen("the hold queue should contain {int} member", [](TestContext& ctx, int expected) -> bool {
        return static_cast<int>(ctx.Get<std::vector<std::string>>("holds").size()) == expected;
    });

    registry.RegisterAnd("{word} should be at the front of the hold queue", [](TestContext& ctx, std::string member) -> bool {
        auto holds = ctx.Get<std::vector<std::string>>("holds");
        return !holds.empty() && holds.front() == member;
    });

    return registry;
}

#endif // BABYBEHAVE_EXAMPLES_GHERKIN_LIBRARY_STEPS_HPP
