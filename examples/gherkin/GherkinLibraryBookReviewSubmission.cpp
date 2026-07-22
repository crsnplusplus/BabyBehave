#include "LoadFeatureFile.hpp"
#include "LibrarySteps.hpp"
#include <BabyBehave/bdd.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::BDD::Gherkin;

// Doc Strings example: a book review submission scenario where a patron
// provides a multi-paragraph review as a Doc String (triple-quoted block).
// The step definition receives the review text via a trailing `const std::string&`
// parameter and stores it in the context for subsequent assertions.
//
// The scenario text lives in examples/gherkin/features/
// library_book_review_submission.feature, read from disk via LoadFeatureFile().

namespace {

// Helper to count words in a string
int CountWords(const std::string& text) {
    int count = 0;
    bool in_word = false;
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++count;
        }
    }
    return count;
}

constexpr Key<std::string> kReviewPatron{"review_patron"};
constexpr Key<std::string> kReviewText{"review_text"};
constexpr Key<bool> kReviewVisible{"review_visible"};

// ---- Review submission with Doc String ----
bool WhenPatronSubmitsReview(TestContext& ctx, std::string patron, const std::string& reviewBody) {
    ctx.Set(kReviewPatron, std::move(patron));
    ctx.Set(kReviewText, reviewBody);
    ctx.Set(kReviewVisible, true);
    return true;
}

// ---- Assertions ----
bool ThenReviewShouldBeVisibleWithAtLeastWords(TestContext& ctx, int minWords) {
    if (!ctx.Get(kReviewVisible)) {
        return false;
    }
    return CountWords(ctx.Get(kReviewText)) >= minWords;
}

bool AndReviewTextShouldContain(TestContext& ctx, std::string searchTerm) {
    // Case-insensitive substring search for more realistic matching
    std::string textLower = ctx.Get(kReviewText);
    std::string termLower = std::move(searchTerm);
    std::transform(textLower.begin(), textLower.end(), textLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(termLower.begin(), termLower.end(), termLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return textLower.find(termLower) != std::string::npos;
}

} // namespace

StepRegistry PrepareRegistry() {
    // Start with the standard library steps, then add review-specific steps
    StepRegistry registry = MakeLibraryStepRegistry();
    registry.RegisterSteps(
        StepEntry{Keyword::When, "a patron {word} submits the following review:", WhenPatronSubmitsReview},
        StepEntry{Keyword::Then, "the review should be visible with at least {int} words",
            ThenReviewShouldBeVisibleWithAtLeastWords},
        StepEntry{Keyword::And, "the review text should contain {string}", AndReviewTextShouldContain});
    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "library_book_review_submission.feature");
}
