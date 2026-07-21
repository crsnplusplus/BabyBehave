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

// Helper to count words in a string
static int CountWords(const std::string& text) {
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

StepRegistry PrepareRegistry() {
    // Start with the standard library steps, then add review-specific steps
    StepRegistry registry = MakeLibraryStepRegistry();

    // ---- Review submission with Doc String ----
    registry.RegisterWhen("a patron {word} submits the following review:",
        [](TestContext& ctx, std::string patron, const std::string& reviewBody) -> bool {
            // Store the review text and patron name in the context
            ctx.Set("review_patron", patron);
            ctx.Set("review_text", reviewBody);
            ctx.Set("review_visible", true);
            return true;
        });

    // ---- Assertions ----
    registry.RegisterThen("the review should be visible with at least {int} words",
        [](TestContext& ctx, int minWords) -> bool {
            if (!ctx.Get<bool>("review_visible")) {
                return false;
            }
            const std::string& reviewText = ctx.Get<std::string>("review_text");
            int wordCount = CountWords(reviewText);
            return wordCount >= minWords;
        });

    registry.RegisterAnd("the review text should contain {string}",
        [](TestContext& ctx, std::string searchTerm) -> bool {
            const std::string& reviewText = ctx.Get<std::string>("review_text");
            // Case-insensitive substring search for more realistic matching
            std::string text_lower = reviewText;
            std::string term_lower = searchTerm;
            std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(term_lower.begin(), term_lower.end(), term_lower.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text_lower.find(term_lower) != std::string::npos;
        });

    return registry;
}

int main() {
    return RunFeatureFromFile(PrepareRegistry(), "library_book_review_submission.feature");
}
