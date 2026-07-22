#ifndef BABYBEHAVE_BDD_HPP
#define BABYBEHAVE_BDD_HPP

#pragma once

#include <algorithm>
#include <functional>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_map>
#include <any>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <utility>
#include <iterator>
#include <limits>
#include <version>
#if defined(__cpp_lib_print)
#include <print>
#endif
#if defined(__cpp_lib_source_location)
#include <source_location>
#endif

// Everything pulled in here is used exclusively by the
// BabyBehave::BDD::Gherkin namespace further down this file (guarded the
// same way there): when BABYBEHAVE_DISABLE_GHERKIN is defined, none of
// these are even included. However, C++20 is still required as the language
// floor (for designated initializers used unconditionally elsewhere in this
// file). Disabling Gherkin only opts out of its additional C++20 library
// dependencies like <regex> and ranges algorithms.
#if !defined(BABYBEHAVE_DISABLE_GHERKIN)
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <regex>
#include <tuple>
#endif


namespace BabyBehave::BDD {

    namespace detail {
        // Cheaper than std::endl (no flush).
        inline constexpr char kNewLine = '\n';

        // Runtime toggle for narration (controlled via BABYBEHAVE_QUIET env var).
        inline bool& NarrationEnabledFlag() {
            static bool enabled = [] {
                const char* env = std::getenv("BABYBEHAVE_QUIET");
                return env == nullptr || *env == '\0' || std::string_view(env) == "0";
            }();
            return enabled;
        }

        inline void PrintLine(const std::string& text) {
            if (!NarrationEnabledFlag()) {
                return;
            }
#if defined(__cpp_lib_print)
            std::println("{}", text);
#else
            std::cout << text << kNewLine;
#endif
        }

        inline void PrintLine() {
            if (!NarrationEnabledFlag()) {
                return;
            }
#if defined(__cpp_lib_print)
            std::println();
#else
            std::cout << kNewLine;
#endif
        }

        // Print diagnostic to std::cerr (respects NarrationEnabledFlag).
        inline void PrintErrorLine(const std::string& text) {
            if (!NarrationEnabledFlag()) {
                return;
            }
            std::cerr << text << kNewLine;
        }

        // Plain (live) vs Arrow/Tree (buffered, rendered at Execute's end).
        enum class NarrationStyle : std::uint8_t {
            Plain,
            Arrow,
            Tree,
        };

        // Maps BABYBEHAVE_STYLE env var to NarrationStyle (defaults to Plain).
        // Pure function for test coverage (each case gets direct test call).
        constexpr NarrationStyle ParseNarrationStyleEnv(const char* env) {
            if (env == nullptr) {
                return NarrationStyle::Plain;
            }
            const std::string_view value(env);
            if (value == "arrow") {
                return NarrationStyle::Arrow;
            }
            if (value == "tree") {
                return NarrationStyle::Tree;
            }
            return NarrationStyle::Plain;
        }

        // Cached from BABYBEHAVE_STYLE env var, overrideable via SetNarrationStyle().
        inline NarrationStyle& NarrationStyleFlag() {
            static NarrationStyle style = ParseNarrationStyleEnv(std::getenv("BABYBEHAVE_STYLE"));
            return style;
        }

        // Step types for Arrow/Tree renderers (Given stored in m_testName).
        enum class StepKindTag : std::uint8_t {
            With,
            When,
            Then,
            And,
            Or,
            But,
        };

        // Detail steps nest under primary: With under GIVEN, And/Or/But under
        // THEN. Arrow marks with "+", Tree nests.
        constexpr bool IsNarrationDetailKind(StepKindTag kind) {
            return kind == StepKindTag::With || kind == StepKindTag::And ||
                   kind == StepKindTag::Or || kind == StepKindTag::But;
        }

        // Exhaustive switch; std::unreachable() means this is unreachable (not just unhandled).
        constexpr std::string_view ArrowKindWord(StepKindTag kind) {
            switch (kind) {
                case StepKindTag::With: return "With";
                case StepKindTag::When: return "When";
                case StepKindTag::Then: return "Then";
                case StepKindTag::And: return "And";
                case StepKindTag::Or: return "Or";
                case StepKindTag::But: return "But";
            }
            std::unreachable();
        }

        constexpr std::string_view TreeKindLabel(StepKindTag kind) {
            switch (kind) {
                case StepKindTag::With: return "WITH";
                case StepKindTag::When: return "WHEN";
                case StepKindTag::Then: return "THEN";
                case StepKindTag::And: return "AND";
                case StepKindTag::Or: return "OR";
                case StepKindTag::But: return "BUT";
            }
            std::unreachable();
        }

        // Single step buffered for Arrow/Tree rendering.
        struct NarrationStepEntry {
            StepKindTag kind;
            std::string name;
        };

        // Joins lines with newlines (lines is never empty).
        inline std::string JoinNarrationLines(const std::vector<std::string>& lines) {
            std::string result = lines.front();
            for (auto it = std::next(lines.begin()); it != lines.end(); ++it) {
                result += kNewLine;
                result += *it;
            }
            return result;
        }

        // Renders "-> Given a: X" / "    + With: Y" etc (details get "+", primary "->").
        inline std::string RenderArrow(const std::string& testName, const std::vector<NarrationStepEntry>& steps, bool passed) {
            std::vector<std::string> lines;
            lines.push_back(std::string(passed ? "[ OK ] " : "[ FAIL ] ") + testName);
            lines.push_back("-> Given a: " + testName);
            for (const auto& entry : steps) {
                std::string line = "    ";
                line += IsNarrationDetailKind(entry.kind) ? "+ " : "-> ";
                line += ArrowKindWord(entry.kind);
                line += ": ";
                line += entry.name;
                lines.push_back(std::move(line));
            }
            return JoinNarrationLines(lines);
        }

        // Left-justifies to 7 chars for alignment.
        inline std::string PadTreeLabel(std::string_view label) {
            constexpr std::size_t kFieldWidth = 7;
            return label.size() < kFieldWidth ? std::string(label) + std::string(kFieldWidth - label.size(), ' ')
                                               : std::string(label);
        }

        // Unicode glyphs as \u escapes (for portable encoding).
        inline constexpr std::string_view kTreeCheckMark = "\u2713";         // \u2713 CHECK MARK
        inline constexpr std::string_view kTreeCrossMark = "\u2717";        // \u2717 BALLOT X
        inline constexpr std::string_view kTreeVerticalBar = "\u2502";      // \u2502 VERTICAL
        inline constexpr std::string_view kTreeBranchMid = "\u251c\u2500 "; // \u251c\u2500 BRANCH MID
        inline constexpr std::string_view kTreeBranchLast = "\u2570\u2500 "; // \u2514\u2500 BRANCH LAST

        // Continuation prefix: "   " if last, else vertical bar + spaces.
        inline std::string TreeContinuation(bool isLast) {
            return isLast ? "   " : std::string(kTreeVerticalBar) + "  ";
        }

        // Top-level branch (WHEN/THEN); THEN may have And/Or/But children.
        struct TreeBranch {
            std::string_view label;
            const std::string* name;
            const std::vector<const NarrationStepEntry*>* children;
        };

        // Appends indented box-drawing lines for children (WITH/And/Or/But).
        // Last child gets arc-corner, others get tee glyph.
        inline void AppendTreeChildLines(std::vector<std::string>& lines, std::string_view indent,
                                          const std::string& continuation,
                                          const std::vector<const NarrationStepEntry*>& children) {
            for (std::size_t i = 0; i < children.size(); ++i) {
                const bool lastChild = (i + 1 == children.size());
                std::string line;
                line += indent;
                line += continuation;
                line += lastChild ? kTreeBranchLast : kTreeBranchMid;
                line += PadTreeLabel(TreeKindLabel(children[i]->kind));
                line += children[i]->name;
                lines.push_back(std::move(line));
            }
        }

        // Unicode tree: GIVEN/WHEN/THEN siblings; WITH under GIVEN;
        // And/Or/But under last THEN (or promoted to top-level).
        inline std::string RenderTree(const std::string& testName, const std::vector<NarrationStepEntry>& steps, bool passed) {
            static constexpr std::string_view kIndent = "  ";
            static const std::vector<const NarrationStepEntry*> kNoChildren;

            std::vector<const NarrationStepEntry*> givenChildren;
            std::vector<const NarrationStepEntry*> whenEntries;
            std::vector<const NarrationStepEntry*> thenEntries;
            std::vector<const NarrationStepEntry*> thenChildren;
            for (const auto& entry : steps) {
                switch (entry.kind) {
                    case StepKindTag::With: givenChildren.push_back(&entry); break;
                    case StepKindTag::When: whenEntries.push_back(&entry); break;
                    case StepKindTag::Then: thenEntries.push_back(&entry); break;
                    default: thenChildren.push_back(&entry); break; // And/Or/But
                }
            }

            std::vector<TreeBranch> branches;
            branches.reserve(whenEntries.size() + std::max(thenEntries.size(), thenChildren.size()));
            for (const auto* entry : whenEntries) {
                branches.push_back(
                    TreeBranch{ .label = TreeKindLabel(StepKindTag::When), .name = &entry->name, .children = &kNoChildren });
            }
            if (thenEntries.empty()) {
                for (const auto* entry : thenChildren) {
                    branches.push_back(
                        TreeBranch{ .label = TreeKindLabel(entry->kind), .name = &entry->name, .children = &kNoChildren });
                }
            } else {
                for (std::size_t i = 0; i < thenEntries.size(); ++i) {
                    const bool isLastThen = (i + 1 == thenEntries.size());
                    branches.push_back(TreeBranch{ .label = TreeKindLabel(StepKindTag::Then),
                                                    .name = &thenEntries[i]->name,
                                                    .children = isLastThen ? &thenChildren : &kNoChildren });
                }
            }
            const bool givenIsLastGroup = branches.empty();

            std::vector<std::string> lines;
            lines.push_back((passed ? "[ " + std::string(kTreeCheckMark) + " OK ] " : "[ " + std::string(kTreeCrossMark) + " FAIL ] ") + testName);
            lines.push_back(std::string(kIndent) + std::string(kTreeVerticalBar));
            lines.push_back(std::string(kIndent) + std::string(givenIsLastGroup ? kTreeBranchLast : kTreeBranchMid) + PadTreeLabel("GIVEN") + testName);

            const std::string givenChildContinuation = TreeContinuation(givenIsLastGroup);
            AppendTreeChildLines(lines, kIndent, givenChildContinuation, givenChildren);

            for (std::size_t i = 0; i < branches.size(); ++i) {
                const bool lastBranch = (i + 1 == branches.size());
                lines.push_back(std::string(kIndent) + std::string(kTreeVerticalBar));
                lines.push_back(std::string(kIndent) + std::string(lastBranch ? kTreeBranchLast : kTreeBranchMid) +
                                 PadTreeLabel(branches[i].label) + *branches[i].name);

                const std::string branchChildContinuation = TreeContinuation(lastBranch);
                AppendTreeChildLines(lines, kIndent, branchChildContinuation, *branches[i].children);
            }
            return JoinNarrationLines(lines);
        }

#if defined(__cpp_lib_source_location)
        // Formats source_location as "file:line".
        inline std::string FormatLocation(const std::source_location& loc) {
            std::string result(loc.file_name());
            result += ':';
            result += std::to_string(loc.line());
            return result;
        }

        // Consolidates source_location capture (replaces #if duplication).
        inline std::string CaptureLocationOrEmpty(const std::source_location& loc) {
            return FormatLocation(loc);
        }
#else
        // Fallback: no <source_location> support. Returns empty location.
        inline std::string CaptureLocationOrEmpty() {
            return std::string();
        }
#endif

        // Always-false trap for StepMeta primary template's static_assert.
        template<typename>
        inline constexpr bool kAlwaysFalseStepType = false;

        // Step-type labels (used as StepResult::stepLabel and exception callback labels).
        inline constexpr std::string_view kPreconditionLabel = "Precondition";
        inline constexpr std::string_view kActionLabel = "Action";
        inline constexpr std::string_view kPostconditionLabel = "Postcondition";
        inline constexpr std::string_view kAndLabel = "And";
        inline constexpr std::string_view kOrLabel = "Or";
        inline constexpr std::string_view kButLabel = "But";
        inline constexpr std::string_view kContextSetupLabel = "ContextSetup";

        inline constexpr std::string_view kUnknownExceptionMessage = "unknown non-std::exception type thrown";
        inline constexpr std::string_view kContextSetupExceptionPrefix = "Exception caught in Context Setup: ";

        // User-facing string literals.
        inline constexpr std::string_view kKeyNotFoundPrefix = "Key not found: ";
        inline constexpr std::string_view kGivenPrefix = "Given a: ";
        inline constexpr std::string_view kWithPrefix = "    With: ";
        inline constexpr std::string_view kWhenPrefix = "    When: ";
        inline constexpr std::string_view kThenPrefix = "    Then: ";
        inline constexpr std::string_view kAndPrefix = "    And: ";
        inline constexpr std::string_view kOrPrefix = "    Or: ";
        inline constexpr std::string_view kButPrefix = "    But: ";
        inline constexpr std::string_view kDefaultExceptionCallbackPrefix = "Exception caught in ";
        inline constexpr std::string_view kOnConditionNotVerifiedCallbackThrewMessage =
            "BabyBehave: onConditionNotVerified callback itself threw an exception; ignoring to avoid std::terminate()";
        inline constexpr std::string_view kOnExceptionCallbackThrewMessage =
            "BabyBehave: onException callback itself threw an exception; ignoring to avoid std::terminate()";

        // Failure messages (zero allocation cost on passing steps).
        inline constexpr std::string_view kPreconditionFailedMessage = "Precondition failed";
        inline constexpr std::string_view kActionFailedMessage = "Action failed";
        inline constexpr std::string_view kPostconditionFailedMessage = "Postcondition failed";
        inline constexpr std::string_view kAndConditionLabel = "And condition";
        inline constexpr std::string_view kAndConditionFailedMessage = "And condition failed";
        inline constexpr std::string_view kOrConditionLabel = "Or condition";
        inline constexpr std::string_view kOrConditionFailedMessage = "Or condition failed";
        inline constexpr std::string_view kButConditionLabel = "But condition";
        inline constexpr std::string_view kButConditionFailedMessage = "But condition failed";

        // Appends " (at file:line)" suffix if location is non-empty.
        inline std::string AppendLocationSuffix(const std::string& text, const std::string& location) {
            return location.empty() ? text : (text + " (at " + location + ")");
        }

        // C++20 transparent hash; Get() avoids materializing std::string.
        struct TransparentStringHash {
            using is_transparent = void;
            std::size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
        };
    } // namespace detail

    // Enable/disable step narration (default: enabled via BABYBEHAVE_QUIET).
    inline void SetNarrationEnabled(bool enabled) {
        detail::NarrationEnabledFlag() = enabled;
    }

    // Public alias for detail::NarrationStyle.
    using NarrationStyle = detail::NarrationStyle;

    // Select narration style (Plain/Arrow/Tree). Process-wide.
    inline void SetNarrationStyle(NarrationStyle style) {
        detail::NarrationStyleFlag() = style;
    }

    // Result of one SoftCheck::Check() call.
    struct SoftCheckResult {
        std::string label;
        bool passed = true;
        std::string message;
    };

    // NOTE: TestContext is NOT thread-safe. Parallel scenarios must not share one.
    class TestContext {
    private:
        std::unordered_map<std::string, std::any, detail::TransparentStringHash, std::equal_to<>> m_objects;

        // SoftCheck results side-channel (written in Check(), cleared before each step).
        std::vector<SoftCheckResult> m_softCheckResults;

        friend class SoftCheck;
        friend class BabyBehaveTest;

        void RecordSoftCheck(SoftCheckResult result) {
            m_softCheckResults.push_back(std::move(result));
        }

        [[nodiscard]] const std::vector<SoftCheckResult>& GetSoftCheckResults() const {
            return m_softCheckResults;
        }

        void ClearSoftCheckResults() {
            m_softCheckResults.clear();
        }

    public:
        // Set via std::string key.
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = std::move(obj);
        }

        // Get via std::string_view key (zero-alloc via transparent hash).
        template<typename T>
        [[nodiscard]] T Get(std::string_view key) const {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                const auto errorMsg = std::string(detail::kKeyNotFoundPrefix) + std::string(key);
                detail::PrintErrorLine(errorMsg);
                throw std::out_of_range(errorMsg);
            }
            return std::any_cast<T>(it->second);
        }

        // Type-safe key variant (compile-time checked).
        template<typename T>
        struct ContextKey {
            const char* name;
        };

        template<typename T>
        void Set(ContextKey<T> key, T obj) {
            Set<T>(std::string(key.name), std::move(obj));
        }

        template<typename T>
        [[nodiscard]] T Get(ContextKey<T> key) const {
            // key.name is const char*; Get<T>(std::string_view) constructs
            // view for free (no allocation).
            return Get<T>(key.name);
        }

        // Returns mutable reference (instead of copy) for in-place mutation
        // of large/expensive-to-copy objects. Same not-found convention as Get<T>.
        template<typename T>
        [[nodiscard]] T& Mutate(std::string_view key) {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                const auto errorMsg = std::string(detail::kKeyNotFoundPrefix) + std::string(key);
                detail::PrintErrorLine(errorMsg);
                throw std::out_of_range(errorMsg);
            }
            return std::any_cast<T&>(it->second);
        }

        // Keyed variant of Mutate<T>(std::string_view) above.
        template<typename T>
        [[nodiscard]] T& Mutate(ContextKey<T> key) {
            return Mutate<T>(key.name);
        }

        // Returns reference to stored value, inserting `init` if absent
        // (never overwrites if key exists, unlike Set<T>).
        template<typename T>
        [[nodiscard]] T& GetOrInit(std::string_view key, T init = T{}) {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                it = m_objects.try_emplace(std::string(key), std::move(init)).first;
            }
            return std::any_cast<T&>(it->second);
        }

        // Keyed variant of GetOrInit<T>(std::string_view) above.
        template<typename T>
        [[nodiscard]] T& GetOrInit(ContextKey<T> key, T init = T{}) {
            return GetOrInit<T>(key.name, std::move(init));
        }
    };

    // Alias for TestContext::ContextKey<T> (e.g., static constexpr Key<int> kCount{"count"};).
    template<typename T>
    using Key = TestContext::ContextKey<T>;

    // Optional recorder for multiple named sub-checks per step (purely additive).
    // Example: SoftCheck checks(ctx); checks.Check("has id", someId > 0);
    // ... checks.Check(...); return checks.AllPassed();
    class SoftCheck {
    public:
        explicit SoftCheck(TestContext& context) : m_context(context) {}

        // Records sub-check and returns condition (supports early returns).
        bool Check(std::string label, bool condition, std::string message = "") {
            m_context.RecordSoftCheck(SoftCheckResult{ .label = std::move(label), .passed = condition, .message = std::move(message) });
            if (!condition) {
                m_allPassed = false;
            }
            return condition;
        }

        // AND of all Check() calls (true if none called).
        [[nodiscard]] bool AllPassed() const {
            return m_allPassed;
        }

    private:
        TestContext& m_context;
        bool m_allPassed = true;
    };

#if defined(__cpp_lib_move_only_function)
    using StepFunction = std::move_only_function<bool(TestContext&)>;
    using ContextSetupFunction = std::move_only_function<void(TestContext&)>;
#else
    using StepFunction = std::function<bool(TestContext&)>;
    using ContextSetupFunction = std::function<void(TestContext&)>;
#endif

    // Callback types (copyable std::function, not move-only).
    using ConditionNotVerifiedCallback = std::function<void(const std::string& errorMsg)>;
    using ExceptionCallback = std::function<void(const std::string& step, const std::exception&)>;

    struct Precondition { StepFunction fn; };
    struct Action { StepFunction fn; };
    struct Postcondition { StepFunction fn; };
    struct And { StepFunction fn; };
    struct Or { StepFunction fn; };
    struct But { StepFunction fn; };

    // StepMeta table eliminates duplicated try/catch logic by indexing
    // per-step-type constants (k*Label, k*FailedMessage).
    namespace detail {
        template<typename T>
        struct StepMeta {
            static_assert(kAlwaysFalseStepType<T>, "Unknown step type");
        };

        template<>
        struct StepMeta<Precondition> {
            static constexpr StepKindTag kind = StepKindTag::With;
            static constexpr std::string_view prefix = kWithPrefix;
            static constexpr std::string_view failedMessage = kPreconditionFailedMessage;
            static constexpr std::string_view resultLabel = kPreconditionLabel;
            static constexpr std::string_view exceptionLabel = kPreconditionLabel;
        };

        template<>
        struct StepMeta<Action> {
            static constexpr StepKindTag kind = StepKindTag::When;
            static constexpr std::string_view prefix = kWhenPrefix;
            static constexpr std::string_view failedMessage = kActionFailedMessage;
            static constexpr std::string_view resultLabel = kActionLabel;
            static constexpr std::string_view exceptionLabel = kActionLabel;
        };

        template<>
        struct StepMeta<Postcondition> {
            static constexpr StepKindTag kind = StepKindTag::Then;
            static constexpr std::string_view prefix = kThenPrefix;
            static constexpr std::string_view failedMessage = kPostconditionFailedMessage;
            static constexpr std::string_view resultLabel = kPostconditionLabel;
            static constexpr std::string_view exceptionLabel = kPostconditionLabel;
        };

        template<>
        struct StepMeta<And> {
            static constexpr StepKindTag kind = StepKindTag::And;
            static constexpr std::string_view prefix = kAndPrefix;
            static constexpr std::string_view failedMessage = kAndConditionFailedMessage;
            static constexpr std::string_view resultLabel = kAndLabel;
            static constexpr std::string_view exceptionLabel = kAndConditionLabel;
        };

        template<>
        struct StepMeta<Or> {
            static constexpr StepKindTag kind = StepKindTag::Or;
            static constexpr std::string_view prefix = kOrPrefix;
            static constexpr std::string_view failedMessage = kOrConditionFailedMessage;
            static constexpr std::string_view resultLabel = kOrLabel;
            static constexpr std::string_view exceptionLabel = kOrConditionLabel;
        };

        template<>
        struct StepMeta<But> {
            static constexpr StepKindTag kind = StepKindTag::But;
            static constexpr std::string_view prefix = kButPrefix;
            static constexpr std::string_view failedMessage = kButConditionFailedMessage;
            static constexpr std::string_view resultLabel = kButLabel;
            static constexpr std::string_view exceptionLabel = kButConditionLabel;
        };
    } // namespace detail

    // Outcome of one executed step (populated in SetCollectFailuresMode).
    // message empty when passed; location is "file:line" if available.
    struct StepResult {
        std::string stepLabel;
        std::string stepName;
        bool passed = true;
        std::string message;
        std::string location;
    };

    // Scenario outcome (allPassed = AND of all StepResult::passed).
    struct TestResult {
        std::string testName;
        bool allPassed = true;
        std::vector<StepResult> steps;
    };

    class BabyBehaveTest {
    public:
        using StepVariant = std::variant<Precondition, Action, Postcondition, And, Or, But>;
        using Step = std::pair<std::string, StepVariant>;


#if defined(__cpp_lib_source_location)
        // loc defaults to caller's location; suppressGivenNarration used by
        // Gherkin for synthetic no-op setup (defaults to false).
        BabyBehaveTest(std::string testName, ContextSetupFunction contextSetupFn,
                        bool suppressGivenNarration = false,
                        const std::source_location& loc = std::source_location::current())
            : m_testName(std::move(testName)),
            m_contextSetupFn(std::move(contextSetupFn)),
            m_contextSetupLocation(detail::CaptureLocationOrEmpty(loc)),
            m_suppressGivenNarration(suppressGivenNarration) {
            InitDefaultCallbacks();
        }
#else
        BabyBehaveTest(std::string testName, ContextSetupFunction contextSetupFn,
                        bool suppressGivenNarration = false)
            : m_testName(std::move(testName)),
            m_contextSetupFn(std::move(contextSetupFn)),
            m_contextSetupLocation(detail::CaptureLocationOrEmpty()),
            m_suppressGivenNarration(suppressGivenNarration) {
            InitDefaultCallbacks();
        }
#endif

        // Only std::bad_alloc escapes; all step/callback exceptions caught internally.
        // NOLINTNEXTLINE(bugprone-exception-escape)
        ~BabyBehaveTest() {
            // Execute() is idempotent.
            Execute();
        }

        BabyBehaveTest(const BabyBehaveTest&) = delete;
        BabyBehaveTest& operator=(const BabyBehaveTest&) = delete;

        // Move deleted: would double-execute (m_executed in both). Copy-elision avoids this anyway.
        BabyBehaveTest(BabyBehaveTest&&) = delete;
        BabyBehaveTest& operator=(BabyBehaveTest&&) = delete;

        void SetOnConditionNotVerifiedCallback(ConditionNotVerifiedCallback callback) {
            m_onConditionNotVerifiedCallback = std::move(callback);
        }

        void SetOnExceptionCallback(ExceptionCallback callback) {
            m_onExceptionCallback = std::move(callback);
        }

        // Record failures to TestResult instead of exit (fluent).
        BabyBehaveTest& SetCollectFailuresMode(bool enabled = true) {
            m_collectFailures = enabled;
            return *this;
        }

#if defined(__cpp_lib_source_location)
        // loc defaults to caller's location; name is sink parameter (moved).
        template<typename StepType>
        BabyBehaveTest& AddStep(std::string name, StepFunction stepFunction,
                                 const std::source_location& loc = std::source_location::current()) {
            return AddStepAt<StepType>(std::move(name), std::move(stepFunction), detail::CaptureLocationOrEmpty(loc));
        }
#else
        template<typename StepType>
        BabyBehaveTest& AddStep(std::string name, StepFunction stepFunction) {
            return AddStepAt<StepType>(std::move(name), std::move(stepFunction), detail::CaptureLocationOrEmpty());
        }
#endif

        // Takes explicit location string (bypasses source_location capture).
        // Used by Gherkin to attribute steps to .feature file/line/column.
        template<typename StepType>
        BabyBehaveTest& AddStepAt(std::string name, StepFunction stepFunction, std::string explicitLocation) {
            StepVariant step = StepType{ std::move(stepFunction) };
            m_steps.emplace_back(std::move(name), std::move(step));
            m_stepLocations.push_back(std::move(explicitLocation));
            return *this;
        }

        const std::vector<Step>& GetSteps() const {
            return m_steps;
        }

        // Runs scenario (setup + steps), returns cached TestResult (idempotent).
        // Normally invoked by destructor; call manually with SetCollectFailuresMode
        // to inspect results before destruction.
        const TestResult& Execute() {
            if (m_executed) {
                return m_result;
            }
            m_executed = true;

            // Plain prints Given immediately; Arrow/Tree buffer for end rendering
            // (suppressed if m_suppressGivenNarration).
            if (!m_suppressGivenNarration && detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
                detail::PrintLine(std::string(detail::kGivenPrefix) + m_testName);
            }
            try {
                m_contextSetupFn(m_context);
            }
            catch (const std::exception& e) {
                VerifyCondition(false, std::string(detail::kContextSetupExceptionPrefix) + e.what(),
                                 detail::kContextSetupLabel, m_testName, m_contextSetupLocation);
            }
            catch (...) {
                VerifyCondition(false, std::string(detail::kContextSetupExceptionPrefix) + std::string(detail::kUnknownExceptionMessage),
                                 detail::kContextSetupLabel, m_testName, m_contextSetupLocation);
            }

            for (std::size_t i = 0; i < m_steps.size(); ++i) {
                auto& step = m_steps[i];
                const std::string& location = m_stepLocations[i];
                std::visit([this, &step, &location](auto&& arg) {
                    executeStep(step.first, arg, location);
                    }, step.second);
            }

            // Plain prints blank; Arrow/Tree render buffered block.
            if (detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
                detail::PrintLine();
            } else {
                const bool passed = !m_anyStepFailedForNarration;
                detail::PrintLine(detail::NarrationStyleFlag() == detail::NarrationStyle::Tree
                                       ? detail::RenderTree(m_testName, m_narrationSteps, passed)
                                       : detail::RenderArrow(m_testName, m_narrationSteps, passed));
                detail::PrintLine();
            }
            return m_result;
        }

        // Return cached TestResult. Meaningful after Execute() is called.
        const TestResult& GetResult() const {
            return m_result;
        }

    private:
        void InitDefaultCallbacks() {
            m_result.testName = m_testName;
            m_onConditionNotVerifiedCallback = [](const std::string& errorMsg) {
                detail::PrintErrorLine(errorMsg);
                std::exit(EXIT_FAILURE);
            };
            m_onExceptionCallback = [](const std::string& step, const std::exception& e) {
                detail::PrintErrorLine(std::string(detail::kDefaultExceptionCallbackPrefix) + step + ":" + e.what());
                std::exit(EXIT_FAILURE);
            };
        }

        // Prints immediately (Plain) or buffers for end rendering (Arrow/Tree).
        void NarrateStep(detail::StepKindTag kind, std::string_view plainPrefix, const std::string& name) {
            if (detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
                detail::PrintLine(std::string(plainPrefix) + name);
            } else {
                m_narrationSteps.push_back(detail::NarrationStepEntry{ .kind = kind, .name = name });
            }
        }

        // One per StepType (template indexed by StepMeta), avoiding try/catch duplication.
        // T& not const: move_only_function::operator() is non-const.
        template<typename T>
        void executeStep(const std::string& name, T& step, const std::string& location) {
            m_context.ClearSoftCheckResults();
            using Meta = detail::StepMeta<T>;
            // StepMeta arguments are compile-time string_view (no allocation).
            NarrateStep(Meta::kind, Meta::prefix, name);
            try {
                VerifyCondition(step.fn(m_context), Meta::failedMessage, Meta::resultLabel, name, location);
            }
            catch (const std::exception& e) {
                SafeInvokeExceptionCallback(Meta::exceptionLabel, e, Meta::resultLabel, name, location);
            }
            catch (...) {
                const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                SafeInvokeExceptionCallback(Meta::exceptionLabel, unknownEx, Meta::resultLabel, name, location);
            }
        }

        // Appends ": label (msg); label2 (msg2)" for failed SoftChecks (avoids allocations).
        void AppendFailedSoftChecks(std::string& out) const {
            bool firstFailure = true;
            for (const auto& check : m_context.GetSoftCheckResults()) {
                if (check.passed) {
                    continue;
                }
                out += firstFailure ? ": " : "; ";
                firstFailure = false;
                out += check.label;
                if (!check.message.empty()) {
                    out += " (" + check.message + ")";
                }
            }
        }

        // Handles step condition: invoke callback or record to m_result on failure.
        // In collect-failures mode, records all steps (skips callback).
        // errorMsg/stepLabel are string_view (no allocation on pass).
        void VerifyCondition(bool condition, std::string_view errorMsg,
                              std::string_view stepLabel, const std::string& stepName,
                              const std::string& location) {
            if (condition) {
                if (m_collectFailures) {
                    m_result.steps.push_back(StepResult{ .stepLabel = std::string(stepLabel), .stepName = stepName, .passed = true, .message = std::string(), .location = location });
                }
                return;
            }
            // Mark failure for Arrow/Tree header rendering.
            m_anyStepFailedForNarration = true;
            std::string augmentedMsg(errorMsg);
            AppendFailedSoftChecks(augmentedMsg);
            if (m_collectFailures) {
                m_result.steps.push_back(StepResult{ .stepLabel = std::string(stepLabel), .stepName = stepName, .passed = false, .message = augmentedMsg, .location = location });
                m_result.allPassed = false;
                return;
            }
            const std::string fullMsg = detail::AppendLocationSuffix(augmentedMsg, location);
            try {
                m_onConditionNotVerifiedCallback(fullMsg);
            }
            catch (...) {
                detail::PrintErrorLine(std::string(detail::kOnConditionNotVerifiedCallbackThrewMessage));
            }
        }

        // Invokes exception callback (same collect-failures behavior as VerifyCondition).
        // Defers "step" label conversion to std::string until callback.
        void SafeInvokeExceptionCallback(std::string_view step, const std::exception& e,
                                          std::string_view stepLabel, const std::string& stepName,
                                          const std::string& location) {
            // Always a failure (only reached from exception handler).
            m_anyStepFailedForNarration = true;
            if (m_collectFailures) {
                m_result.steps.push_back(StepResult{ .stepLabel = std::string(stepLabel), .stepName = stepName, .passed = false, .message = std::string(e.what()), .location = location });
                m_result.allPassed = false;
                return;
            }
            try {
                if (location.empty()) {
                    m_onExceptionCallback(std::string(step), e);
                } else {
                    const std::runtime_error locatedEx(detail::AppendLocationSuffix(e.what(), location));
                    m_onExceptionCallback(std::string(step), locatedEx);
                }
            }
            catch (...) {
                detail::PrintErrorLine(std::string(detail::kOnExceptionCallbackThrewMessage));
            }
        }

        std::string m_testName;
        ContextSetupFunction m_contextSetupFn;
        std::string m_contextSetupLocation;

        // Suppresses redundant "Given a: <name>" for Gherkin synthetic no-op setup.
        bool m_suppressGivenNarration = false;

        TestContext m_context;
        std::vector<Step> m_steps;
        std::vector<std::string> m_stepLocations;

        // Steps buffered for Arrow/Tree rendering (empty under Plain).
        std::vector<detail::NarrationStepEntry> m_narrationSteps;
        bool m_anyStepFailedForNarration = false;

        ConditionNotVerifiedCallback m_onConditionNotVerifiedCallback;
        ExceptionCallback m_onExceptionCallback;

        bool m_collectFailures = false;
        bool m_executed = false;
        TestResult m_result;
    };


#if defined(__cpp_lib_source_location)
    // loc captures the Given/GivenA macro call site.
    inline BabyBehaveTest GivenAImpl(std::string testName,
        ContextSetupFunction contextSetup,
        const std::source_location& loc = std::source_location::current()) {
        return { std::move(testName), std::move(contextSetup), false, loc };
    }
#else
    inline BabyBehaveTest GivenAImpl(std::string testName,
        ContextSetupFunction contextSetup) {
        return { std::move(testName), std::move(contextSetup) };
    }
#endif

    // Short macro aliases. Define BABYBEHAVE_NO_SHORT_MACROS to disable and use
    // AddStep<StepType>/GivenAImpl directly if they collide with other code.
#ifndef BABYBEHAVE_NO_SHORT_MACROS
#define Given(func)  GivenAImpl(#func, {func})
#define GivenA(func) GivenAImpl(#func, {func})
#define With(func)  AddStep<Precondition>(#func, {func})
#define When(func)  AddStep<Action>(#func, {func})
#define Then(func)  AddStep<Postcondition>(#func, {func})
#define And(func)   AddStep<And>(#func, {func})
#define But(func)   AddStep<But>(#func, {func})
#define Or(func)    AddStep<Or>(#func, {func})
    // -I aliases (WithI/WhenI/etc) reuse base macro to avoid divergence bugs.
    // Given/GivenA are separate (independently-documented entry points).
#define WithI(func) With(func)
#define WhenI(func) When(func)
#define ThenI(func) Then(func)
#define AndI(func)  And(func)
#define ButI(func)  But(func)
#define OrI(func)   Or(func)
#endif // BABYBEHAVE_NO_SHORT_MACROS

    // =========================================================================
    // Gherkin support (v0.8.0, opt-out via BABYBEHAVE_DISABLE_GHERKIN; C++20+)
    // =========================================================================
    // Runtime interpreter: parses .feature text, dispatches steps to StepRegistry,
    // drives BabyBehaveTest machinery. Covered: Feature, Background, Scenario/Example,
    // Scenario Outline/Template + Examples/Scenarios (expanded at parse time into
    // ordinary Scenarios - see impl::ExpandScenarioOutlines), steps, @tags,
    // # comments, {int}/{float}/{string}/{word}, Before/After hooks (tag-filtered,
    // AND/subset match), Data Tables (pipe-row step arguments - see
    // impl::HandleDataTableLine), Doc Strings ("""-delimited step arguments -
    // see impl::HandleDocStringLine). NOT covered (parse error): Rule,
    // i18n keywords.
#if !defined(BABYBEHAVE_DISABLE_GHERKIN)
namespace Gherkin {

    // void(TestContext&), wrapped to StepFunction via impl::WrapHookAsStep.
    using HookFunction = std::function<void(TestContext&)>;

    // void(), for StepRegistry::AddBeforeAllHook/AddAfterAllHook (Suite-level
    // hooks - see those methods below). Deliberately NOT HookFunction
    // (void(TestContext&)): a Before-ALL hook runs once per RunFeature() call,
    // before ANY Scenario exists, and an After-ALL hook runs once after every
    // Scenario's own TestContext has already been destroyed - there is no
    // single Scenario's TestContext to hand either one, since each Scenario
    // gets its own fresh TestContext (see impl::RunScenarioAttempt). Unlike
    // per-Scenario hooks, Suite-level hooks are also never tag-filtered (they
    // are unconditional and run exactly once regardless of which Scenarios/
    // tags the Feature contains), so there is no tags/expression pair to
    // store alongside them either - plain std::function<void()>, no wrapper
    // struct needed.
    using SuiteHookFunction = std::function<void()>;

    // Invoked once per Gherkin-sourced failure (a parse error, or a failing
    // Scenario) - never once-per-RunFeature-call, so plain std::function (not
    // move_only_function) is required: it may be invoked multiple times
    // across a single RunFeature() call (once per failing Scenario). Default
    // is impl::DefaultGherkinFailureAction (print + std::exit), preserving
    // today's fail-hard behavior for consumers who don't pass one.
    using GherkinFailureCallback = std::function<void(std::string_view)>;

    // A step's pipe-delimited data table (see docs/design/gherkin-support.md,
    // "Data Tables"): a pipe-row block immediately following a step line in
    // a .feature file, with row 0 conventionally treated as the header by
    // Header()/Get(). The type itself is just a raw grid - nothing enforces
    // "header-ness" semantically, matching Cucumber's own DataTable
    // philosophy; a step definition that doesn't care about a header can
    // index `rows` directly. A step definition opts into receiving one by
    // declaring a trailing `const DataTable&` parameter beyond its
    // {int}/{float}/{string}/{word} placeholders - see StepRegistry's
    // arity-based raw-argument dispatch (impl::StepDefinitionRawArgKind).
    struct DataTable {
        std::vector<std::vector<std::string>> rows;

        // Number of DATA rows, i.e. excluding the header row (rows[0]).
        // 0 for an empty table (rows.empty()).
        [[nodiscard]] std::size_t RowCount() const {
            return rows.empty() ? 0 : rows.size() - 1;
        }

        // Header row (rows[0]). Throws std::out_of_range if rows is empty.
        [[nodiscard]] const std::vector<std::string>& Header() const {
            return rows.at(0);
        }

        // Data row by 0-based index (rows[dataIdx + 1]). Throws
        // std::out_of_range if dataIdx >= RowCount().
        [[nodiscard]] const std::vector<std::string>& Row(std::size_t dataIdx) const {
            return rows.at(dataIdx + 1);
        }

        // Cell value by data row index + header column name (header-aware
        // lookup). Throws std::invalid_argument if columnName is not one of
        // Header()'s cells (bad input the caller controls - matches this
        // file's existing convention, e.g. CompileStepPattern's unknown-
        // placeholder throw); std::out_of_range if dataIdx >= RowCount().
        [[nodiscard]] std::string Get(std::size_t dataIdx, std::string_view columnName) const {
            const std::vector<std::string>& header = Header();
            const auto headerIt = std::ranges::find(header, columnName);
            if (headerIt == header.end()) {
                throw std::invalid_argument(
                    "BabyBehave::Gherkin: DataTable has no column '" + std::string(columnName) + "'");
            }
            const auto columnIndex = static_cast<std::size_t>(std::distance(header.begin(), headerIt));
            return Row(dataIdx).at(columnIndex);
        }
    };

    // Internal parsing/matching (not part of public surface). Named impl (not
    // detail) to avoid shadowing BDD::detail with unqualified lookups.
    namespace impl {

        // --- Text-splitting helpers -----------------------------------------
        // Manual find/substr (not std::ranges::split which is broken for
        // std::string_view on real compilers). Not generators: parsing isn't hot path.

        inline std::vector<std::string_view> SplitLines(std::string_view text) {
            std::vector<std::string_view> lines;
            std::size_t start = 0;
            while (true) {
                const std::size_t newlinePos = text.find('\n', start);
                if (newlinePos == std::string_view::npos) {
                    lines.push_back(text.substr(start));
                    break;
                }
                std::size_t len = newlinePos - start;
                if (len > 0 && text[start + len - 1] == '\r') {
                    --len; // tolerate CRLF line endings
                }
                lines.push_back(text.substr(start, len));
                start = newlinePos + 1;
            }
            return lines;
        }

        inline bool IsAsciiSpace(char letter) {
            return std::isspace(static_cast<unsigned char>(letter)) != 0;
        }

        inline std::size_t LeadingWhitespaceCount(std::string_view text) {
            std::size_t index = 0;
            while (index < text.size() && IsAsciiSpace(text[index])) {
                ++index;
            }
            return index;
        }

        inline std::string_view TrimView(std::string_view text) {
            const std::size_t begin = LeadingWhitespaceCount(text);
            std::size_t end = text.size();
            while (end > begin && IsAsciiSpace(text[end - 1])) {
                --end;
            }
            return text.substr(begin, end - begin);
        }

        // Un-escapes a single already-split pipe-row cell: Cucumber's
        // standard Data Table/Examples table cell-escaping, '\|' is a
        // literal pipe (not a column delimiter) and '\\' is a literal
        // backslash; any other backslash is copied through unchanged (no
        // "unknown escape" error - deliberately permissive, matching this
        // parser's general "when in doubt, don't add a new failure mode"
        // stance elsewhere, e.g. SubstitutePlaceholders' unmatched <name>).
        inline std::string UnescapePipeCell(std::string_view cell) {
            std::string out;
            out.reserve(cell.size());
            for (std::size_t i = 0; i < cell.size(); ++i) {
                if (cell[i] == '\\' && i + 1 < cell.size() && (cell[i + 1] == '|' || cell[i + 1] == '\\')) {
                    out += cell[i + 1];
                    ++i;
                } else {
                    out += cell[i];
                }
            }
            return out;
        }

        // Split a "| a | b | c |"-shaped trimmed line into {"a", "b", "c"},
        // trimming and un-escaping (see UnescapePipeCell) each cell.
        // General-purpose (not Examples-specific): also reused by Data
        // Tables (see impl::HandleDataTableLine), so it must stay agnostic
        // of what the caller does with the resulting cells. Leading '|' is
        // assumed already present (the caller only invokes this after
        // checking trimmedLine.front() == '|'); anything after a missing/
        // trailing final '|' is silently dropped. A '|' immediately
        // preceded by an ODD number of backslashes is an escaped literal
        // pipe (part of the cell, not a delimiter) - '\|' escapes one pipe,
        // '\\|' is a literal backslash followed by a real delimiter, and so
        // on, matching Cucumber's own escaping convention.
        inline std::vector<std::string> ParsePipeRow(std::string_view trimmedLine) {
            std::vector<std::string> cells;
            std::size_t index = (!trimmedLine.empty() && trimmedLine.front() == '|') ? 1 : 0;
            while (index <= trimmedLine.size()) {
                std::size_t pipePos = index;
                for (;;) {
                    pipePos = trimmedLine.find('|', pipePos);
                    if (pipePos == std::string_view::npos) {
                        break;
                    }
                    std::size_t precedingBackslashes = 0;
                    while (precedingBackslashes < pipePos &&
                           trimmedLine[pipePos - precedingBackslashes - 1] == '\\') {
                        ++precedingBackslashes;
                    }
                    if (precedingBackslashes % 2 == 0) {
                        break; // Not escaped: a real column delimiter.
                    }
                    ++pipePos; // Escaped '|': keep scanning past it.
                }
                if (pipePos == std::string_view::npos) {
                    break;
                }
                cells.emplace_back(UnescapePipeCell(TrimView(trimmedLine.substr(index, pipePos - index))));
                index = pipePos + 1;
            }
            return cells;
        }

        // Append @tokens (leading @ stripped) to tags; non-@ tokens skipped.
        inline void AppendTagsFromLine(std::string_view line, std::vector<std::string>& tags) {
            std::size_t index = 0;
            while (index < line.size()) {
                while (index < line.size() && IsAsciiSpace(line[index])) {
                    ++index;
                }
                const std::size_t start = index;
                while (index < line.size() && !IsAsciiSpace(line[index])) {
                    ++index;
                }
                if (index > start) {
                    const std::string_view token = line.substr(start, index - start);
                    if (token.size() > 1 && token.front() == '@') {
                        tags.emplace_back(token.substr(1));
                    }
                }
            }
        }

        // --- Step keyword + cucumber-expression-lite pattern matching -------

        enum class StepKeyword : std::uint8_t { Given, When, Then, And, But };
        inline constexpr std::size_t kStepKeywordCount = 5;  // Number of enumerators

        inline std::optional<std::pair<StepKeyword, std::string_view>> MatchStepKeyword(std::string_view line) {
            static constexpr std::array<std::pair<std::string_view, StepKeyword>, kStepKeywordCount> kKeywords{ {
                { "Given ", StepKeyword::Given },
                { "When ", StepKeyword::When },
                { "Then ", StepKeyword::Then },
                { "And ", StepKeyword::And },
                { "But ", StepKeyword::But },
            } };
            for (const auto& [prefix, keyword] : kKeywords) {
                // C++20 starts_with (already this header's effective floor).
                if (line.starts_with(prefix)) {
                    return std::make_pair(keyword, line.substr(prefix.size()));
                }
            }
            return std::nullopt;
        }

        // Pattern like "I add {int} apples" → regex ^I\ add\ (-?[0-9]+)\ apples$.
        // Escapes all regex-metacharacters so literals never become accidental groups.
        struct CompiledStepPattern {
            std::regex regex;
            std::size_t placeholderCount = 0;
        };

        inline std::string EscapeRegexLiteral(std::string_view text) {
            static constexpr std::string_view kSpecialChars = R"(.^$|()[]{}*+?\)";
            std::string out;
            out.reserve(text.size());
            for (const char letter : text) {
                if (kSpecialChars.find(letter) != std::string_view::npos) {
                    out += '\\';
                }
                out += letter;
            }
            return out;
        }

        inline CompiledStepPattern CompileStepPattern(std::string_view pattern) {
            std::string regexStr = "^";
            std::size_t placeholderCount = 0;
            std::size_t index = 0;
            while (index < pattern.size()) {
                if (pattern[index] == '{') {
                    const std::size_t close = pattern.find('}', index);
                    if (close == std::string_view::npos) {
                        regexStr += EscapeRegexLiteral(pattern.substr(index));
                        break;
                    }
                    const std::string_view placeholder = pattern.substr(index + 1, close - index - 1);
                    if (placeholder == "int") {
                        regexStr += "(-?[0-9]+)";
                    } else if (placeholder == "float") {
                        regexStr += R"((-?[0-9]+(?:\.[0-9]+)?))";
                    } else if (placeholder == "string") {
                        // Custom raw-string delimiter ("re") needed: content contains )".
                        regexStr += R"re("([^"]*)")re";
                    } else if (placeholder == "word") {
                        regexStr += R"((\S+))";
                    } else {
                        // Registration-time error (not .feature-parse-time).
                        throw std::invalid_argument(
                            "BabyBehave::Gherkin: unknown step placeholder '{" + std::string(placeholder) + "}'");
                    }
                    ++placeholderCount;
                    index = close + 1;
                } else {
                    const std::size_t next = pattern.find('{', index);
                    const std::string_view literal =
                        (next == std::string_view::npos) ? pattern.substr(index) : pattern.substr(index, next - index);
                    regexStr += EscapeRegexLiteral(literal);
                    index = (next == std::string_view::npos) ? pattern.size() : next;
                }
            }
            regexStr += "$";
            return CompiledStepPattern{ .regex = std::regex(regexStr), .placeholderCount = placeholderCount };
        }

        // --- Captured-string -> C++ type conversion -------------------------

        template<typename>
        inline constexpr bool kAlwaysFalseGherkinType = false;

        // Dispatches on std::remove_cvref_t<T> (not T itself): a step
        // definition parameter is USUALLY declared by value (int, double,
        // std::string, ...), but the trailing raw-argument sink for a Doc
        // String may legitimately be declared either `std::string` (by
        // value) or `const std::string&` (by reference - see
        // StepDefinitionRawArgKind/RawArgumentKind::DocString) - MakeStepThunk's
        // None-branch instantiates this same function template for EVERY
        // parameter type in F's ArgsTuple regardless of which runtime
        // branch actually executes (it's a runtime `if`, not `if constexpr`
        // - see MakeStepThunk), so a purely T-based dispatch would fail to
        // even COMPILE for a `const std::string&`-typed trailing parameter
        // (T doesn't equal std::string, it equals a reference TO one).
        // Returning `raw` as T when T is `const std::string&` is safe:
        // `raw` itself is a reference into the caller's `captures` vector,
        // which outlives this call.
        template<typename T>
        T ConvertCapture(const std::string& raw) {
            using Decayed = std::remove_cvref_t<T>;
            if constexpr (std::is_reference_v<T>) {
                // Reference-typed capture parameters are ONLY safe for
                // std::string: `raw` is itself a reference into the
                // caller's already-alive `captures` vector, so returning
                // it back out as `const std::string&` is fine. A numeric
                // reference (e.g. `const int&`) has no such backing
                // storage - std::stoi(raw) et al. produce a function-local
                // temporary, and returning a reference to it would be a
                // dangling reference (undefined behavior) as soon as this
                // function returns. Hard-stop those at compile time
                // instead of silently handing back a dangling reference.
                static_assert(std::is_same_v<Decayed, std::string>,
                    "ConvertCapture: reference-typed capture parameters are only supported for std::string "
                    "(e.g. Doc String arguments); numeric captures must be declared by value "
                    "(int/long/long long/float/double), not by reference.");
                return raw;
            } else if constexpr (std::is_same_v<Decayed, int>) {
                return std::stoi(raw);
            } else if constexpr (std::is_same_v<Decayed, long>) {
                return std::stol(raw);
            } else if constexpr (std::is_same_v<Decayed, long long>) {
                return std::stoll(raw);
            } else if constexpr (std::is_same_v<Decayed, double>) {
                return std::stod(raw);
            } else if constexpr (std::is_same_v<Decayed, float>) {
                return std::stof(raw);
            } else if constexpr (std::is_same_v<Decayed, std::string>) {
                return raw;
            } else {
                static_assert(kAlwaysFalseGherkinType<T>,
                    "Unsupported step-definition parameter type; use int/long/long long/float/double/std::string");
                return T{};
            }
        }

        // --- Raw step arguments: Data Tables and Doc Strings -----------------
        // A step definition may declare ONE extra trailing parameter beyond
        // its {int}/{float}/{string}/{word} placeholder captures, to receive
        // whatever raw block (a Data Table or a Doc String) is attached to
        // the .feature step. See
        // StepDefinitionRawArgKind below for the arity-based dispatch that
        // decides, per step definition, whether/which trailing parameter it
        // has; this is deliberately BY ARITY (parameter count), not by
        // scanning for a magic type, so today's steps (no trailing raw
        // parameter) are completely unaffected.
        enum class RawArgumentKind : std::uint8_t {
            None,       // No trailing raw-argument parameter (today's/default behavior).
            DataTable,  // Trailing `const DataTable&` parameter.
            DocString,  // Trailing `const std::string&` parameter (Doc Strings).
        };

        // std::monostate: no raw argument attached to this step invocation
        // (RawArgumentKind::None, or a step with a raw-argument-capable
        // definition that simply has none attached in the .feature file).
        using RawArgument = std::variant<std::monostate, DataTable, std::string>;

        // --- Compile-time callable-signature deduction ----------------------
        // Concept/SFINAE-based parameter deduction (no generic lambdas supported).

        template<typename F, typename = void>
        struct HasCallOperator : std::false_type {};
        template<typename F>
        struct HasCallOperator<F, std::void_t<decltype(&F::operator())>> : std::true_type {};

        template<typename F>
        struct CallableSignature : CallableSignature<decltype(&F::operator())> {};

        template<typename C, typename R, typename... Args>
        struct CallableSignature<R (C::*)(Args...) const> {
            using ArgsTuple = std::tuple<Args...>;
        };
        template<typename C, typename R, typename... Args>
        struct CallableSignature<R (C::*)(Args...)> {
            using ArgsTuple = std::tuple<Args...>;
        };
        template<typename R, typename... Args>
        struct CallableSignature<R (*)(Args...)> {
            using ArgsTuple = std::tuple<Args...>;
        };

        // Validate F's shape and return placeholder parameter count.
        template<typename F>
        constexpr std::size_t StepDefinitionArgCount() {
            using Decayed = std::decay_t<F>;
            static_assert(HasCallOperator<Decayed>::value || std::is_pointer_v<Decayed>,
                "Step definition must be a plain function pointer or a non-generic lambda/functor with a single "
                "operator() (generic/templated lambdas are not supported)");
            using ArgsTuple = CallableSignature<Decayed>::ArgsTuple;
            static_assert(std::tuple_size_v<ArgsTuple> >= 1,
                "Step definition must take TestContext& as its first parameter");
            // Use std::remove_cvref_t (C++20 standard library).
            static_assert(std::is_same_v<std::remove_cvref_t<std::tuple_element_t<0, ArgsTuple>>, TestContext>,
                "Step definition's first parameter must be TestContext&");
            return std::tuple_size_v<ArgsTuple> - 1;
        }

        // Determine whether F has a trailing raw-argument parameter beyond
        // its `placeholderCount` {int}/{float}/{string}/{word} captures
        // (placeholderCount comes from CompileStepPattern, i.e. a RUNTIME
        // value parsed from the pattern string - it cannot be a template
        // parameter), and if so, which kind:
        //   - N == placeholderCount            -> RawArgumentKind::None
        //     (today's/default behavior, completely unaffected).
        //   - N == placeholderCount + 1        -> the LAST parameter (index
        //     N-1) is UNCONDITIONALLY the raw-argument sink, never a
        //     placeholder capture; its type must be EXACTLY `const
        //     DataTable&` or exactly `const std::string&` (checked on the
        //     RAW parameter type, not std::remove_cvref_t of it - a mutable
        //     `DataTable&`/by-value `DataTable`/`DataTable&&` is rejected
        //     here too, matching what this function's own error message
        //     promises), or this throws std::invalid_argument at
        //     registration time (mirrors CompileStepPattern's
        //     unknown-placeholder throw style). Rejecting the mismatch here
        //     - rather than std::remove_cvref_t-normalizing it away - turns
        //     a BY-VALUE `DataTable`/`std::string` trailing parameter (which
        //     MakeStepThunk would otherwise happily bind via an implicit
        //     copy from its internal `const DataTable&`/`const std::string&`
        //     - see InvokeWithCapturesAndRaw) into this clean, immediate
        //     std::invalid_argument instead of a silently-accepted
        //     deviation from the documented contract. It does NOT change
        //     what happens for a MUTABLE `DataTable&`/`std::string&`
        //     trailing parameter specifically - MakeStepThunk<F> is still
        //     unconditionally instantiated by AddStepDefinition regardless
        //     of what this function returns/throws (a runtime throw cannot
        //     suppress a sibling template instantiation), and binding
        //     MakeStepThunk's internal `const DataTable&` to a non-const
        //     `DataTable&` parameter is already a hard compile error on its
        //     own, with or without this check.
        //   - Any other N                      -> RawArgumentKind::None,
        //     leaving AddStepDefinition's existing placeholder-count-
        //     mismatch check (compiled.placeholderCount != expectedArgs) to
        //     report the error exactly as it always has - this function
        //     must never mask that error.
        // N is F's own parameter count excluding TestContext& (the same
        // tuple StepDefinitionArgCount already inspects), i.e. compile-time
        // from F alone.
        template<typename F>
        RawArgumentKind StepDefinitionRawArgKind(std::size_t placeholderCount) {
            using Decayed = std::decay_t<F>;
            using ArgsTuple = CallableSignature<Decayed>::ArgsTuple;
            constexpr std::size_t paramCount = std::tuple_size_v<ArgsTuple> - 1;
            if (paramCount != placeholderCount + 1) {
                return RawArgumentKind::None;
            }
            // Deliberately NOT std::remove_cvref_t here (see the comment
            // block above): F's trailing parameter must be exactly `const
            // DataTable&`/`const std::string&`, so this checks the raw,
            // unmodified parameter type from ArgsTuple.
            using LastParam = std::tuple_element_t<std::tuple_size_v<ArgsTuple> - 1, ArgsTuple>;
            if constexpr (std::is_same_v<LastParam, const DataTable&>) {
                return RawArgumentKind::DataTable;
            } else if constexpr (std::is_same_v<LastParam, const std::string&>) {
                return RawArgumentKind::DocString;
            } else {
                throw std::invalid_argument(
                    "BabyBehave::Gherkin: step definition's trailing parameter (beyond its {int}/{float}/{string}/"
                    "{word} placeholders) must be exactly 'const DataTable&' or 'const std::string&'");
            }
        }

        template<typename ArgsTuple, typename F, std::size_t... I>
        bool InvokeWithCaptures(F& func, TestContext& ctx, const std::vector<std::string>& captures,
                                  std::index_sequence<I...> seq) {
            std::ignore = seq;
            return static_cast<bool>(func(ctx, ConvertCapture<std::tuple_element_t<I + 1, ArgsTuple>>(captures[I])...));
        }

        // Same as InvokeWithCaptures, but also binds the trailing raw-argument
        // parameter after the placeholder captures. `raw` is already typed
        // (DataTable or std::string) - unlike captures, it is NOT passed
        // through ConvertCapture.
        template<typename ArgsTuple, typename F, typename RawType, std::size_t... I>
        bool InvokeWithCapturesAndRaw(F& func, TestContext& ctx, const std::vector<std::string>& captures,
                                        const RawType& raw, std::index_sequence<I...> seq) {
            std::ignore = seq;
            return static_cast<bool>(
                func(ctx, ConvertCapture<std::tuple_element_t<I + 1, ArgsTuple>>(captures[I])..., raw));
        }

        // Handles MakeStepThunk's rawArgKind == RawArgumentKind::None case -
        // i.e. every one of F's own parameters is an ordinary
        // {int}/{float}/{string}/{word}-style capture. Split out of
        // MakeStepThunk purely to keep that function's cognitive complexity
        // under this codebase's clang-tidy threshold; behaviorally this is
        // still just "the None case" of the same dispatch. lastParamIsDataTable
        // is a template parameter (not recomputed here) so the caller's
        // single already-computed value is reused, keeping this an exact
        // extraction rather than a behavior change.
        template<typename ArgsTuple, std::size_t paramCount, bool lastParamIsDataTable, typename F>
        bool InvokeStepNoRawArg(F& stepFn, TestContext& ctx, const std::vector<std::string>& captures) {
            if constexpr (!lastParamIsDataTable) {
                if (captures.size() != paramCount) {
                    detail::PrintErrorLine("BabyBehave::Gherkin: internal error - captured argument count "
                                             "does not match step definition");
                    return false;
                }
                return InvokeWithCaptures<ArgsTuple>(stepFn, ctx, captures, std::make_index_sequence<paramCount>{});
            } else {
                // Reachable only via a narrow misuse: StepDefinitionRawArgKind
                // returns None (rather than DataTable) whenever F's own
                // parameter count equals placeholderCount exactly, i.e. the
                // user wrote a step function whose LAST parameter is typed
                // exactly DataTable but registered it against a pattern
                // whose {int}/{float}/{string}/{word} placeholder count
                // equals F's TOTAL parameter count (treating DataTable as
                // if it were just another placeholder capture, instead of
                // one fewer placeholder so DataTable is the trailing raw-
                // argument sink). That specific mismatch is NOT caught by
                // AddStepDefinition's placeholder-count check (placeholderCount
                // does equal the expected arg count) nor by
                // StepDefinitionRawArgKind's registration-time throw (which
                // only fires for the N == placeholderCount + 1 case), so it
                // falls through to this runtime internal-error print instead
                // of a clean registration-time diagnostic.
                detail::PrintErrorLine(
                    "BabyBehave::Gherkin: internal error - step definition's trailing DataTable parameter "
                    "was invoked with RawArgumentKind::None");
                return false;
            }
        }

        // Handles MakeStepThunk's rawArgKind != None case - F's trailing
        // parameter binds from `rawArgument` (see StepDefinitionRawArgKind)
        // after the placeholder captures. Split out of MakeStepThunk for the
        // same cognitive-complexity reason as InvokeStepNoRawArg above.
        // rawArgKind != None implies paramCount >= 1 AND F's own trailing
        // parameter type is exactly DataTable or exactly std::string (see
        // StepDefinitionRawArgKind, which is the ONLY place a non-None
        // rawArgKind is ever produced, and which already validated this at
        // registration time). The `if constexpr` chain below is keyed on F's
        // ACTUAL trailing parameter type (not just paramCount >= 1): this is
        // not just an optimization, it is REQUIRED for this to even compile -
        // InvokeWithCapturesAndRaw<..., DataTable, ...> calls func(...,
        // DataTable) as its last argument, which must not be instantiated
        // for an F whose last parameter is e.g. `double` (a plain
        // placeholder-only step definition), even though such an F's
        // rawArgKind is always None at runtime and this whole function is
        // then dead code for it.
        template<typename ArgsTuple, std::size_t paramCount, typename F>
        bool InvokeStepWithRawArg(F& stepFn, TestContext& ctx, const std::vector<std::string>& captures,
                                    const RawArgument& rawArgument) {
            if constexpr (paramCount >= 1) {
                using LastParam = std::remove_cvref_t<std::tuple_element_t<paramCount, ArgsTuple>>;
                constexpr std::size_t capturesN = paramCount - 1;
                if constexpr (std::is_same_v<LastParam, DataTable>) {
                    if (captures.size() != capturesN) {
                        detail::PrintErrorLine(
                            "BabyBehave::Gherkin: internal error - captured argument count does not match step definition");
                        return false;
                    }
                    if (!std::holds_alternative<DataTable>(rawArgument)) {
                        detail::PrintErrorLine("BabyBehave::Gherkin: internal error - step definition expects a "
                                                 "data table but none is attached");
                        return false;
                    }
                    return InvokeWithCapturesAndRaw<ArgsTuple>(stepFn, ctx, captures, std::get<DataTable>(rawArgument),
                                                                 std::make_index_sequence<capturesN>{});
                } else if constexpr (std::is_same_v<LastParam, std::string>) {
                    if (captures.size() != capturesN) {
                        detail::PrintErrorLine(
                            "BabyBehave::Gherkin: internal error - captured argument count does not match step definition");
                        return false;
                    }
                    if (!std::holds_alternative<std::string>(rawArgument)) {
                        detail::PrintErrorLine(
                            "BabyBehave::Gherkin: internal error - step definition expects a doc string but none is attached");
                        return false;
                    }
                    return InvokeWithCapturesAndRaw<ArgsTuple>(stepFn, ctx, captures, std::get<std::string>(rawArgument),
                                                                 std::make_index_sequence<capturesN>{});
                } else {
                    // Unreachable in practice: StepDefinitionRawArgKind
                    // only ever returns a non-None kind when LastParam is
                    // DataTable or std::string. Kept so every F still
                    // compiles.
                    detail::PrintErrorLine("BabyBehave::Gherkin: internal error - step definition's trailing "
                                             "parameter is not a recognized raw-argument type");
                    return false;
                }
            } else {
                // Unreachable in practice: AddStepDefinition only ever
                // computes a non-None rawArgKind when paramCount >= 1.
                // Kept so this branch still compiles for every F.
                detail::PrintErrorLine(
                    "BabyBehave::Gherkin: internal error - step definition takes no parameters but a raw "
                    "argument kind was set");
                return false;
            }
        }

        // Type-erase step definition to bool(TestContext&, captures, rawArgument).
        // Converts captures to F's parameter types before invoking; when
        // rawArgKind != None, also binds F's trailing DataTable/std::string
        // parameter from `rawArgument` (see StepDefinitionRawArgKind).
        // When rawArgKind == None, this is STRUCTURALLY identical to
        // pre-Feature-4 behavior: it calls the exact same, unmodified
        // InvokeWithCaptures - the same code path, not merely "the same
        // observable result". The lambda's own branching is intentionally
        // kept to a single if/return dispatching into InvokeStepNoRawArg/
        // InvokeStepWithRawArg above - both were split out of this function
        // purely to keep MakeStepThunk's cognitive complexity under this
        // codebase's clang-tidy threshold.
        template<typename F>
        std::function<bool(TestContext&, const std::vector<std::string>&, const RawArgument&)>
        MakeStepThunk(F stepFn, RawArgumentKind rawArgKind) {
            using ArgsTuple = CallableSignature<std::decay_t<F>>::ArgsTuple;
            constexpr std::size_t paramCount = std::tuple_size_v<ArgsTuple> - 1;
            // Whether F's own trailing parameter is exactly DataTable - the
            // only type ConvertCapture doesn't support (std::string IS
            // supported, so a trailing std::string is fine either way). If
            // so, the None-branch's call below (which would ConvertCapture
            // EVERY parameter, including the last) must not even be
            // instantiated: MakeStepThunk<F> is instantiated exactly ONCE
            // per F, so its generated lambda body must compile for every
            // branch below regardless of which one rawArgKind actually
            // selects at runtime.
            constexpr bool lastParamIsDataTable =
                paramCount >= 1 &&
                std::is_same_v<std::remove_cvref_t<std::tuple_element_t<paramCount, ArgsTuple>>, DataTable>;
            return [stepFn = std::move(stepFn), rawArgKind](TestContext& ctx, const std::vector<std::string>& captures,
                                                              const RawArgument& rawArgument) mutable -> bool {
                if (rawArgKind == RawArgumentKind::None) {
                    return InvokeStepNoRawArg<ArgsTuple, paramCount, lastParamIsDataTable>(stepFn, ctx, captures);
                }
                return InvokeStepWithRawArg<ArgsTuple, paramCount>(stepFn, ctx, captures, rawArgument);
            };
        }

        // --- Tag expressions (Feature 7: AND/OR/NOT hook matching) -----------
        // A second, parallel way to filter Before/After hooks by tag, alongside
        // the original vector-of-tags AND/subset match (TagsAreSubsetOf below).
        // A tag expression is a small boolean grammar over @tag operands:
        //   expression := or_expr
        //   or_expr    := and_expr ('or' and_expr)*
        //   and_expr   := not_expr ('and' not_expr)*
        //   not_expr   := 'not' not_expr | primary
        //   primary    := '(' or_expr ')' | tag
        // Operator precedence (highest to lowest): not > and > or. Parentheses
        // group explicitly. "and"/"or"/"not" keywords are case-insensitive.
        // Tags are written "@name"; the leading '@' is stripped when stored in
        // TagExpressionNode::tagName, matching how AppendTagsFromLine already
        // strips '@' from parsed .feature file tags. Registered via
        // StepRegistry::AddBeforeHookExpr/AddAfterHookExpr below, which parse
        // eagerly (at registration time) and throw std::invalid_argument on
        // any malformed expression - mirrors CompileStepPattern's
        // registration-time throw style (see its unknown-placeholder throw).

        enum class TagExprOp : std::uint8_t { Tag, And, Or, Not };

        // AST node for a parsed tag expression. shared_ptr (not unique_ptr) so
        // Hook - and therefore StepRegistry - stays copyable (StepRegistry's
        // copy/move special members are explicitly defaulted; see StepRegistry
        // below). When op==Tag, tagName holds the tag (leading '@' stripped);
        // left/right are unused. When op==Not, left holds the operand; right
        // is unused. When op==And/Or, left and right hold the two operands.
        // Always fully populated by ParseTagExpression - EvaluateTagExpression
        // below dereferences left/right unconditionally.
        struct TagExpressionNode {
            TagExprOp op;
            std::string tagName;
            std::shared_ptr<TagExpressionNode> left;
            std::shared_ptr<TagExpressionNode> right;
        };

        enum class TagExprTokenType : std::uint8_t { Tag, Keyword, LParen, RParen, Eof };

        struct TagExprToken {
            TagExprTokenType type;
            std::string value;  // tag text (with '@') or lowercased keyword; empty otherwise
            std::size_t pos;    // offset into the original expression, for error messages
        };

        inline bool IsTagExpressionKeywordChar(char letter) {
            return std::isalpha(static_cast<unsigned char>(letter)) != 0;
        }
        inline bool IsTagExpressionTagChar(char letter) {
            return std::isspace(static_cast<unsigned char>(letter)) == 0 && letter != '(' && letter != ')';
        }

        // Lexes a "@tagName" token starting at expression[index] (index
        // points at the '@'). Advances index past the consumed tag text.
        // Throws std::invalid_argument if '@' isn't followed by at least one
        // tag character. Split out of TokenizeTagExpression below purely to
        // keep that function's cognitive complexity under this codebase's
        // clang-tidy threshold - behaviorally it's still just "the '@' case".
        inline TagExprToken LexTagExpressionTag(std::string_view expression, std::size_t& index) {
            const std::size_t startPos = index;
            const std::size_t tagStart = index;
            ++index;
            while (index < expression.size() && IsTagExpressionTagChar(expression[index])) {
                ++index;
            }
            if (index == tagStart + 1) {
                throw std::invalid_argument("BabyBehave::Gherkin: tag expression error at position " +
                    std::to_string(startPos) + ": '@' must be followed by a tag name");
            }
            return TagExprToken{ .type = TagExprTokenType::Tag,
                                  .value = std::string(expression.substr(tagStart, index - tagStart)),
                                  .pos = startPos };
        }

        // Lexes an "and"/"or"/"not" keyword token (case-insensitive) starting
        // at expression[index]. Advances index past the consumed identifier.
        // Throws std::invalid_argument for any other identifier - which is
        // also how a tag written without its required leading '@' (e.g.
        // "foo" instead of "@foo") gets rejected. Split out of
        // TokenizeTagExpression below for the same cognitive-complexity
        // reason as LexTagExpressionTag above.
        inline TagExprToken LexTagExpressionKeyword(std::string_view expression, std::size_t& index) {
            const std::size_t startPos = index;
            const std::size_t kwStart = index;
            while (index < expression.size() && IsTagExpressionKeywordChar(expression[index])) {
                ++index;
            }
            std::string keyword = std::string(expression.substr(kwStart, index - kwStart));
            std::ranges::transform(keyword, keyword.begin(), [](char letter) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            });
            if (keyword != "and" && keyword != "or" && keyword != "not") {
                throw std::invalid_argument("BabyBehave::Gherkin: tag expression error at position " +
                    std::to_string(startPos) + ": unrecognized keyword '" + keyword +
                    "' (expected 'and', 'or', or 'not')");
            }
            return TagExprToken{ .type = TagExprTokenType::Keyword, .value = keyword, .pos = startPos };
        }

        // Tokenizes a tag expression. Throws std::invalid_argument
        // (registration-time error, mirroring CompileStepPattern) on any
        // unrecognized character, a bare '@' with no tag name after it, or an
        // identifier that isn't "and"/"or"/"not" (case-insensitive) - see
        // LexTagExpressionTag/LexTagExpressionKeyword above for the '@' and
        // identifier cases respectively.
        inline std::vector<TagExprToken> TokenizeTagExpression(std::string_view expression) {
            std::vector<TagExprToken> tokens;
            std::size_t index = 0;

            while (index < expression.size()) {
                while (index < expression.size() && std::isspace(static_cast<unsigned char>(expression[index])) != 0) {
                    ++index;
                }
                if (index >= expression.size()) {
                    break;
                }

                const std::size_t startPos = index;
                const char letter = expression[index];

                if (letter == '(') {
                    tokens.push_back(TagExprToken{ .type = TagExprTokenType::LParen, .value = "", .pos = startPos });
                    ++index;
                } else if (letter == ')') {
                    tokens.push_back(TagExprToken{ .type = TagExprTokenType::RParen, .value = "", .pos = startPos });
                    ++index;
                } else if (letter == '@') {
                    tokens.push_back(LexTagExpressionTag(expression, index));
                } else if (IsTagExpressionKeywordChar(letter)) {
                    tokens.push_back(LexTagExpressionKeyword(expression, index));
                } else {
                    throw std::invalid_argument(
                        "BabyBehave::Gherkin: tag expression error at position " + std::to_string(startPos) +
                        ": unrecognized character '" + std::string(1, letter) + "'");
                }
            }

            tokens.push_back(TagExprToken{ .type = TagExprTokenType::Eof, .value = "", .pos = expression.size() });
            return tokens;
        }

        // Recursive-descent parser for the grammar documented above. Throws
        // std::invalid_argument on any syntax error.
        class TagExpressionParser {
        public:
            explicit TagExpressionParser(std::vector<TagExprToken> tokens) : m_tokens(std::move(tokens)) {}

            [[nodiscard]] std::shared_ptr<TagExpressionNode> Parse() {
                if (Check(TagExprTokenType::Eof)) {
                    throw std::invalid_argument("BabyBehave::Gherkin: tag expression cannot be empty");
                }
                std::shared_ptr<TagExpressionNode> result = ParseOr();
                if (!Check(TagExprTokenType::Eof)) {
                    throw std::invalid_argument("BabyBehave::Gherkin: tag expression error at position " +
                        std::to_string(Peek().pos) + ": unexpected token after expression");
                }
                return result;
            }

        private:
            std::vector<TagExprToken> m_tokens;
            std::size_t m_current = 0;

            [[nodiscard]] const TagExprToken& Peek() const {
                return (m_current < m_tokens.size()) ? m_tokens[m_current] : m_tokens.back();
            }
            [[nodiscard]] bool Check(TagExprTokenType type) const {
                return Peek().type == type;
            }
            const TagExprToken& Consume() {
                const TagExprToken& tok = Peek();
                if (m_current < m_tokens.size()) {
                    ++m_current;
                }
                return tok;
            }

            [[nodiscard]] std::shared_ptr<TagExpressionNode> ParseOr() {
                std::shared_ptr<TagExpressionNode> left = ParseAnd();
                while (Check(TagExprTokenType::Keyword) && Peek().value == "or") {
                    std::ignore = Consume();
                    std::shared_ptr<TagExpressionNode> right = ParseAnd();
                    left = std::make_shared<TagExpressionNode>(
                        TagExpressionNode{ .op = TagExprOp::Or, .tagName = "", .left = left, .right = right });
                }
                return left;
            }

            [[nodiscard]] std::shared_ptr<TagExpressionNode> ParseAnd() {
                std::shared_ptr<TagExpressionNode> left = ParseNot();
                while (Check(TagExprTokenType::Keyword) && Peek().value == "and") {
                    std::ignore = Consume();
                    std::shared_ptr<TagExpressionNode> right = ParseNot();
                    left = std::make_shared<TagExpressionNode>(
                        TagExpressionNode{ .op = TagExprOp::And, .tagName = "", .left = left, .right = right });
                }
                return left;
            }

            [[nodiscard]] std::shared_ptr<TagExpressionNode> ParseNot() {
                if (Check(TagExprTokenType::Keyword) && Peek().value == "not") {
                    std::ignore = Consume();
                    // Right-associative: "not not @x" is valid (double-negation).
                    std::shared_ptr<TagExpressionNode> operand = ParseNot();
                    return std::make_shared<TagExpressionNode>(
                        TagExpressionNode{ .op = TagExprOp::Not, .tagName = "", .left = operand, .right = nullptr });
                }
                return ParsePrimary();
            }

            [[nodiscard]] std::shared_ptr<TagExpressionNode> ParsePrimary() {
                if (Check(TagExprTokenType::LParen)) {
                    std::ignore = Consume();
                    std::shared_ptr<TagExpressionNode> expr = ParseOr();
                    if (!Check(TagExprTokenType::RParen)) {
                        throw std::invalid_argument("BabyBehave::Gherkin: tag expression error at position " +
                            std::to_string(Peek().pos) + ": expected ')' to close parenthesized expression");
                    }
                    std::ignore = Consume();
                    return expr;
                }
                if (Check(TagExprTokenType::Tag)) {
                    const TagExprToken& tagToken = Consume();
                    return std::make_shared<TagExpressionNode>(TagExpressionNode{
                        .op = TagExprOp::Tag, .tagName = tagToken.value.substr(1), .left = nullptr, .right = nullptr });
                }
                throw std::invalid_argument("BabyBehave::Gherkin: tag expression error at position " +
                    std::to_string(Peek().pos) + ": expected a tag, 'not', or '(' but found '" + Peek().value + "'");
            }
        };

        // Parses a tag expression string (e.g. "@smoke and not @slow") into an
        // AST. Throws std::invalid_argument at registration time (called from
        // StepRegistry::AddBeforeHookExpr/AddAfterHookExpr) on any malformed
        // expression - see grammar/error-message documentation above.
        inline std::shared_ptr<TagExpressionNode> ParseTagExpression(std::string_view expression) {
            TagExpressionParser parser(TokenizeTagExpression(expression));
            return parser.Parse();
        }

        // Evaluates a parsed tag expression against a Scenario's effective
        // tags (already '@'-stripped, matching how TagExpressionNode::tagName
        // is stored - see ParsePrimary above).
        inline bool EvaluateTagExpression(const TagExpressionNode& node, const std::vector<std::string>& availableTags) {
            switch (node.op) {
                case TagExprOp::Tag:
                    return std::ranges::find(availableTags, node.tagName) != availableTags.end();
                case TagExprOp::Not:
                    return !EvaluateTagExpression(*node.left, availableTags);
                case TagExprOp::And:
                    return EvaluateTagExpression(*node.left, availableTags) &&
                           EvaluateTagExpression(*node.right, availableTags);
                case TagExprOp::Or:
                    return EvaluateTagExpression(*node.left, availableTags) ||
                           EvaluateTagExpression(*node.right, availableTags);
            }
            return false;  // Unreachable; silences a compiler warning on some toolchains.
        }

        // --- Before/After hooks ----------------------------------------------

        // Wrap HookFunction to StepFunction via always-true wrapper.
        inline StepFunction WrapHookAsStep(std::function<void(TestContext&)> hookFn) {
            return [hookFn = std::move(hookFn)](TestContext& ctx) mutable -> bool {
                hookFn(ctx);
                return true;
            };
        }

        // Store copyable HookFunction (not move-only StepFunction).
        // WrapHookAsStep() called fresh per Scenario to produce new StepFunction.
        // A Hook is either vector-tags-based (`tags` populated, possibly
        // empty meaning "always run"; `expression` unset - today's/default
        // behavior, completely unaffected by Feature 7) OR expression-based
        // (Feature 7: `expression` set via StepRegistry::AddBeforeHookExpr/
        // AddAfterHookExpr, `tags` left empty) - NEVER both. See
        // MatchesHookTags below for the dispatch.
        struct Hook {
            std::vector<std::string> tags;
            HookFunction fn;
            std::string label;
            std::optional<std::shared_ptr<TagExpressionNode>> expression;
        };

        struct StepDefinition {
            CompiledStepPattern pattern;
            std::string patternText;
            std::function<bool(TestContext&, const std::vector<std::string>&, const RawArgument&)> thunk;
            RawArgumentKind rawArgKind = RawArgumentKind::None;
        };

        // --- Tags: union (Feature -> Scenario inheritance) and AND/subset match --

        inline std::vector<std::string> UnionTags(const std::vector<std::string>& featureTags,
                                                    const std::vector<std::string>& scenarioTags) {
            std::vector<std::string> result = featureTags;
            for (const auto& tag : scenarioTags) {
                if (std::ranges::find(result, tag) == result.end()) {
                    result.push_back(tag);
                }
            }
            return result;
        }

        // AND/subset only (no OR/NOT): all tags in required must be in available.
        inline bool TagsAreSubsetOf(const std::vector<std::string>& required, const std::vector<std::string>& available) {
            return std::ranges::all_of(required, [&](const std::string& tag) {
                return std::ranges::find(available, tag) != available.end();
            });
        }

        // Dispatches a Hook's tag-matching, per the vector-tags-based-XOR-
        // expression-based contract documented on Hook above: an
        // expression-based hook (Feature 7, `expression` populated) evaluates
        // its parsed AND/OR/NOT AST via EvaluateTagExpression; a vector-tags-
        // based hook (today's/default behavior, `expression` unset) keeps
        // using TagsAreSubsetOf, byte-identical to before Feature 7.
        inline bool MatchesHookTags(const Hook& hook, const std::vector<std::string>& availableTags) {
            if (hook.expression.has_value()) {
                return EvaluateTagExpression(**hook.expression, availableTags);
            }
            return TagsAreSubsetOf(hook.tags, availableTags);
        }

        inline std::string JoinTagsForLabel(const std::vector<std::string>& tags) {
            if (tags.empty()) {
                return "(always)";
            }
            std::string out;
            for (std::size_t i = 0; i < tags.size(); ++i) {
                if (i != 0) {
                    out += ",";
                }
                out += "@" + tags[i];
            }
            return out;
        }

        // --- Timeout annotations (@timeout:<value><unit>) ---------------------
        // No parser change: AppendTagsFromLine already stores "timeout:5s" as a
        // normal tag string (leading '@' stripped) in ParsedScenario::tags/
        // ParsedFeature::tags, same as any other tag. It is interpreted only
        // here, when computing a Scenario's execution policy, and it is
        // deliberately NOT stripped out of effectiveTags afterward (kept
        // simple; harmless if a hook happens to filter on it too).

        // Unit-conversion constants (named, not bare magic numbers - this
        // codebase's clang-tidy config is WarningsAsErrors: '*', which
        // includes readability-magic-numbers).
        inline constexpr std::int64_t kMillisecondsPerSecond = 1000;
        inline constexpr std::int64_t kSecondsPerMinute = 60;
        inline constexpr std::int64_t kMillisecondsPerMinute = kMillisecondsPerSecond * kSecondsPerMinute;

        inline constexpr std::string_view kTimeoutTagPrefix = "timeout:";
        inline constexpr std::string_view kTimeoutAnnotationLabel = "Timeout";
        inline constexpr std::string_view kTimeoutAnnotationStepName = "@timeout annotation";
        inline constexpr std::string_view kTimeoutExceededMessage = "BabyBehave::Gherkin: scenario exceeded @timeout deadline";

        // @retry:N (Feature 6, retry/flaky annotations) - sibling tag to
        // @timeout, same "reuse the plain @token tag grammar, no parser
        // change" approach. "Retry" is used as the generic annotation-parse-
        // error label/step-name too (see ScenarioExecutionPolicy::parseError
        // below - it is shared between the @timeout and @retry parse paths,
        // so the label/step-name recorded alongside a parse error is chosen
        // per-error rather than hardcoded to one annotation only).
        inline constexpr std::string_view kRetryTagPrefix = "retry:";
        inline constexpr std::string_view kRetryAnnotationLabel = "Retry";
        inline constexpr std::string_view kRetryAnnotationStepName = "@retry annotation";
        inline constexpr std::size_t kDefaultMaxAttempts = 1;

        // Execution policy computed from a Scenario's effective tags (Feature
        // -> Scenario union; see UnionTags). @timeout and @retry today.
        struct ScenarioExecutionPolicy {
            std::optional<std::chrono::milliseconds> timeout;
            // Total attempts (NOT retries) a Scenario should run: 1 == no
            // retry (today's default/unaffected behavior), N == retry up to
            // N-1 times after an initial failure, stopping at the first
            // success. Populated by an "@retry:N" tag; see ParseRetryValue.
            std::size_t maxAttempts = kDefaultMaxAttempts;
            // Non-empty => malformed @timeout or @retry annotation.
            // RunScenarioWithRetries must fail the scenario immediately in
            // this case (once, not per-attempt - a parse error is a static
            // defect in the annotation itself, not a flaky runtime failure,
            // so retrying it would just fail identically every time), before
            // any Before hook/Background/step/After hook runs.
            std::string parseError;
            // Which annotation produced parseError, for an accurate
            // stepLabel/stepName on the synthetic failing StepResult -
            // see RunScenarioWithRetries.
            std::string_view parseErrorLabel;
            std::string_view parseErrorStepName;
        };

        // Tracks a Scenario's wall-clock start and, if set, its @timeout
        // budget. Expired() is checked cooperatively between steps only -
        // see WrapWithDeadlineCheck for the full safety contract.
        struct ScenarioDeadline {
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            std::optional<std::chrono::milliseconds> timeout;

            [[nodiscard]] bool Expired() const {
                return timeout.has_value() && (std::chrono::steady_clock::now() - start) > *timeout;
            }
        };

        // Parses a "<value><unit>" timeout tag value (e.g. "5s"/"500ms"/"2m").
        // A unit suffix is REQUIRED (s, ms, or m) - a bare unitless number
        // (e.g. "@timeout:5") is deliberately rejected rather than defaulting
        // to a unit, to avoid any ambiguity about what "5" means. Checks "ms"
        // before "s" since "ms" also ends with 's'. On success returns the
        // parsed duration; on any malformed input (non-numeric value, missing
        // or unrecognized unit, zero, or negative) returns nullopt and sets error.
        inline std::optional<std::chrono::milliseconds> ParseTimeoutValue(std::string_view value, std::string& error) {
            struct UnitSuffix {
                std::string_view suffix;
                std::int64_t msPerUnit;
            };
            static constexpr std::array<UnitSuffix, 3> kUnitSuffixes = { {
                { .suffix = "ms", .msPerUnit = 1 },
                { .suffix = "s", .msPerUnit = kMillisecondsPerSecond },
                { .suffix = "m", .msPerUnit = kMillisecondsPerMinute },
            } };
            for (const auto& unit : kUnitSuffixes) {
                if (!value.ends_with(unit.suffix)) {
                    continue;
                }
                const std::string_view numberPart = value.substr(0, value.size() - unit.suffix.size());
                if (numberPart.empty()) {
                    continue; // No digits before the unit suffix; not a match after all.
                }
                std::int64_t parsedValue = 0;
                const auto* begin = numberPart.data();
                const auto* end = std::next(begin, static_cast<std::ptrdiff_t>(numberPart.size()));
                const auto [ptr, ec] = std::from_chars(begin, end, parsedValue);
                if (ec != std::errc() || ptr != end) {
                    continue; // Not fully numeric; try the next candidate suffix.
                }
                if (parsedValue <= 0) {
                    error = "@timeout value must be a positive, non-zero duration (got '" + std::string(value) + "')";
                    return std::nullopt;
                }
                // Guard the multiplication below against overflow (UB on a
                // pathological huge "@timeout:...m" value) - reject as
                // malformed rather than silently wrapping.
                if (parsedValue > (std::numeric_limits<std::int64_t>::max() / unit.msPerUnit)) {
                    error = "@timeout value '" + std::string(value) + "' is too large to represent";
                    return std::nullopt;
                }
                return std::chrono::milliseconds(parsedValue * unit.msPerUnit);
            }
            error = "malformed @timeout value '" + std::string(value) +
                     "' (expected an integer followed by a required unit suffix: s, ms, or m)";
            return std::nullopt;
        }

        // Parses a "<N>" retry tag value (e.g. "3"). N must be a positive
        // integer >= 1 - "@retry:1" is accepted (it just means "run once, no
        // retry", the same as omitting the tag entirely) but "@retry:0" and
        // any negative or non-numeric value are rejected, mirroring
        // ParseTimeoutValue's "fail fast, clear message" philosophy above. On
        // success returns the parsed attempt count; on malformed input
        // returns nullopt and sets error.
        inline std::optional<std::size_t> ParseRetryValue(std::string_view value, std::string& error) {
            std::int64_t parsedValue = 0;
            const auto* begin = value.data();
            const auto* end = std::next(begin, static_cast<std::ptrdiff_t>(value.size()));
            const auto [ptr, ec] = std::from_chars(begin, end, parsedValue);
            if (ec != std::errc() || ptr != end) {
                error = "malformed @retry value '" + std::string(value) +
                         "' (expected a positive integer total-attempt count, e.g. '@retry:3')";
                return std::nullopt;
            }
            if (parsedValue <= 0) {
                error = "@retry value must be a positive integer (got '" + std::string(value) +
                         "') - @retry:N is the TOTAL number of attempts, not a count of extra retries, so it "
                         "cannot be zero or negative";
                return std::nullopt;
            }
            return static_cast<std::size_t>(parsedValue);
        }

        // Scans effectiveTags for every "timeout:"/"retry:" tag and keeps the
        // LAST match of each. Precedence rationale: UnionTags concatenates
        // Feature tags first, then appends whichever Scenario tags aren't
        // already present (see UnionTags above) - so the last "timeout:"/
        // "retry:" tag in effectiveTags is always the Scenario's own, if it
        // set one, and only falls back to the Feature's otherwise. This makes
        // the more specific (Scenario) annotation win over the less specific
        // (Feature) one, matching how tag inheritance conceptually ought to
        // work. If no "timeout:" tag is present at all, policy.timeout stays
        // nullopt (no timeout enforcement); if no "retry:" tag is present at
        // all, policy.maxAttempts stays at its default of 1 (no retry) -
        // both are today's exact pre-existing behavior, unaffected, when
        // their respective tag is absent. @timeout is checked before @retry
        // below purely for a deterministic error when a Scenario somehow has
        // both malformed at once; there is no other significance to the order.
        inline ScenarioExecutionPolicy ParseScenarioExecutionPolicy(const std::vector<std::string>& effectiveTags) {
            ScenarioExecutionPolicy policy;
            std::string_view lastTimeoutValue;
            bool haveTimeoutTag = false;
            std::string_view lastRetryValue;
            bool haveRetryTag = false;
            for (const auto& tag : effectiveTags) {
                const std::string_view tagView(tag);
                if (tagView.starts_with(kTimeoutTagPrefix)) {
                    lastTimeoutValue = tagView.substr(kTimeoutTagPrefix.size());
                    haveTimeoutTag = true;
                } else if (tagView.starts_with(kRetryTagPrefix)) {
                    lastRetryValue = tagView.substr(kRetryTagPrefix.size());
                    haveRetryTag = true;
                }
            }
            if (haveTimeoutTag) {
                std::string error;
                const std::optional<std::chrono::milliseconds> parsed = ParseTimeoutValue(lastTimeoutValue, error);
                if (!parsed) {
                    policy.parseError = error;
                    policy.parseErrorLabel = kTimeoutAnnotationLabel;
                    policy.parseErrorStepName = kTimeoutAnnotationStepName;
                    return policy;
                }
                policy.timeout = parsed;
            }
            if (haveRetryTag) {
                std::string error;
                const std::optional<std::size_t> parsed = ParseRetryValue(lastRetryValue, error);
                if (!parsed) {
                    policy.parseError = error;
                    policy.parseErrorLabel = kRetryAnnotationLabel;
                    policy.parseErrorStepName = kRetryAnnotationStepName;
                    return policy;
                }
                policy.maxAttempts = *parsed;
            }
            return policy;
        }

        // Wraps a StepFunction with a cooperative, INTER-STEP deadline check:
        // before invoking `inner`, checks deadline->Expired(); if expired,
        // THROWS std::runtime_error (not `return false`) so a clear "scenario
        // exceeded @timeout deadline" message flows into StepResult::message
        // via the same exception-catching path BabyBehaveTest::executeStep
        // already has for every step (collect-failures mode is forced on in
        // RunScenario) - this is the only channel that gets a custom,
        // human-readable message into TestResult for this case, as opposed to
        // the generic per-step-type message (e.g. "Action failed") a plain
        // `return false` would produce. If not expired, calls through to
        // `inner` normally and returns its result.
        //
        // SAFETY LIMITATION - deliberate, not a bug: this is COOPERATIVE,
        // INTER-STEP checking ONLY. It does not, and cannot, interrupt a
        // single hung/blocking step already in progress: there is no
        // background thread racing the step, and none is added here on
        // purpose, because RunScenario binds `const&`/`std::string_view`
        // parameters into the caller's stack frame - forcibly aborting a step
        // from another thread would leave those dangling and be undefined
        // behavior. A deadline can only stop the scenario from *starting* its
        // next step once the deadline has already passed; a step that itself
        // blocks past the deadline still runs to completion before this check
        // is reached again. Once the deadline HAS expired, EVERY subsequent
        // wrapped step also throws immediately (its real body never runs),
        // but each still produces its own distinct failed StepResult - this
        // is consistent with this codebase's already-documented difference
        // from real Cucumber (see docs/design/gherkin-support.md, "forced
        // collect-failures mode": "BabyBehave's Gherkin mode executes and
        // reports every step, even after an earlier failure").
        inline StepFunction WrapWithDeadlineCheck(StepFunction inner, std::shared_ptr<ScenarioDeadline> deadline) {
            return [inner = std::move(inner), deadline = std::move(deadline)](TestContext& ctx) mutable -> bool {
                if (deadline->Expired()) {
                    throw std::runtime_error(std::string(kTimeoutExceededMessage));
                }
                return inner(ctx);
            };
        }

        // --- Parser ----------------------------------------------------------
        // std::string_view in, no file I/O (header-only design).

        struct ParsedStep {
            StepKeyword keyword = StepKeyword::Given;
            std::string text;
            std::size_t line = 0;
            std::size_t column = 0;
            // std::monostate by default: no Data Table/Doc String attached.
            // Populated by ProcessFeatureLine's Data Table ('|' row -
            // see HandleDataTableLine) or Doc String ('"""'-delimited block -
            // see HandleDocStringLine) handling when one immediately follows
            // this step.
            RawArgument rawArgument;
        };

        // One data row of an Examples:/Scenarios: table; line is the row's
        // OWN source line (not the header's), for accurate "which data
        // failed" diagnostics once the row is expanded into a Scenario.
        struct ExamplesRow {
            std::size_t line = 0;
            std::vector<std::string> cells;
        };

        // A Scenario Outline's/Scenario Template's Examples:/Scenarios: table.
        struct ExamplesTable {
            std::vector<std::string> header;
            std::size_t headerLine = 0;
            std::vector<ExamplesRow> rows;
        };

        struct ParsedScenario {
            std::string name;
            std::vector<std::string> tags;
            std::vector<ParsedStep> steps;
            std::size_t line = 0;
            // Populated only for a Scenario Outline/Template; nullopt for
            // every plain Scenario:/Example: (including every expanded
            // per-row Scenario produced by ExpandScenarioOutlines - expanded
            // rows are deliberately indistinguishable from hand-written
            // Scenarios downstream of expansion).
            std::optional<ExamplesTable> examples;
        };

        struct ParsedFeature {
            std::string name;
            std::vector<std::string> tags;
            std::vector<ParsedStep> background;
            std::vector<ParsedScenario> scenarios;
        };

        // Plain ok/errors/value result (not std::expected; no chaining
        // needed). errors is a VECTOR (not a single string) as of the
        // multi-error-accumulation feature: ParseFeatureText no longer
        // stops at the first structural parse error - it keeps parsing the
        // rest of the text, recording every distinct structural error it
        // finds along the way, and only sets ok=false if that vector ends
        // up non-empty. See RecordParseError below (the single place every
        // Handle*Line error path funnels through) and ParseFeatureText's own
        // doc comment for the full recovery-strategy rationale (point vs.
        // intermediate vs. corruptive errors).
        struct ParseOutcome {
            bool ok = false;
            std::vector<std::string> errors;
            ParsedFeature feature;
        };

        // Move in-progress Scenario to feature.scenarios (called between sections + end).
        inline void FlushScenario(ParsedFeature& feature, std::optional<ParsedScenario>& currentScenario) {
            if (currentScenario) {
                feature.scenarios.push_back(std::move(*currentScenario));
                currentScenario.reset();
            }
        }

        // Which step a just-parsed .feature line is eligible to attach a Data
        // Table to: identified by container (Background vs the in-progress
        // Scenario) + INDEX (not a pointer/reference - an index survives a
        // later, unrelated push_back reallocating feature.background or
        // currentScenario->steps, unlike a raw pointer into either would).
        struct StepTarget {
            bool inBackground = false;
            std::size_t index = 0;
        };

        // Mutable parse state (one per ParseFeatureText call).
        struct FeatureParseState {
            ParsedFeature feature;
            // Every structural parse error found so far, in discovery order
            // (see RecordParseError, the sole place anything is ever pushed
            // here). Moved into ParseOutcome::errors once parsing finishes -
            // see ParseFeatureText.
            std::vector<std::string> errors;
            std::vector<std::string> pendingTags;
            std::optional<ParsedScenario> currentScenario;
            bool inBackground = false;
            bool haveFeature = false;
            // Scenario Outline/Examples state (see FinalizeCurrentScenarioExamples).
            bool inExamplesTable = false;
            bool haveExamplesHeader = false;
            std::optional<ExamplesTable> pendingExamples;
            bool currentScenarioIsOutline = false;
            // Data Table state (see HandleDataTableLine). lastStepTarget is
            // set right after every successful step-attach (in
            // ProcessFeatureLine) to the just-attached step; it survives
            // blank/comment/tag/free-text lines (this codebase's Examples:
            // table parsing already tolerates blank lines between rows - see
            // ProcessFeatureLine's '|' handling - so Data Tables follow the
            // same convention), but is reset whenever the Background:/
            // Scenario:/Feature:/Examples: CONTEXT changes (a stale index
            // into the wrong container would otherwise be a real bug), and
            // is left untouched (NOT cleared) once a table is attached to
            // it - inspecting the step's own rawArgument (already
            // monostate or not) is what lets a second '|' block right after
            // the first be reported as "step already has an argument"
            // rather than "no preceding step". inDataTable tracks whether
            // the immediately-preceding line was itself a Data Table row
            // (reset on every non-'|' line), i.e. whether the NEXT '|' line
            // continues that same table or starts a fresh evaluation.
            bool inDataTable = false;
            // INTERMEDIATE-recovery flag (see RecordParseError/
            // ParseFeatureText's recovery-strategy doc comment): set when a
            // Data Table row is rejected ("data table with no preceding
            // step", "step already has an argument", or a stray
            // Examples:/Scenarios: header with no preceding Outline) so
            // every REMAINING contiguous '|'-prefixed line of that same
            // malformed table is silently skipped (see ProcessFeatureLine)
            // instead of re-reporting the identical error once per row -
            // cleared on the first non-'|' line, exactly like inDataTable.
            bool skipMalformedTableLines = false;
            std::optional<StepTarget> lastStepTarget;
            // Doc String state (see HandleDocStringLine). Reuses
            // lastStepTarget exactly as Data Tables do (same target, same
            // "no preceding step"/"step already has an argument" checks -
            // whichever of a Data Table or a Doc String is attached first
            // wins the step's single rawArgument slot). Unlike a Data
            // Table's inDataTable (which only spans consecutive '|' lines
            // and lets any other line end/re-evaluate it), inDocString spans
            // an explicit open/close pair of '"""' lines: EVERY line in
            // between - blank, '#'-prefixed, '|'-prefixed, anything - is
            // accumulated as literal Doc String content verbatim and is
            // NEVER run through the normal blank/comment/tag/table/keyword
            // classification below (see ProcessFeatureLine's inDocString
            // short-circuit, checked before any other line handling).
            // docStringIndent is the opening '"""' line's own leading-
            // whitespace column (LeadingWhitespaceCount(raw) at that line),
            // stripped from every content line once the block closes (up to
            // that many leading whitespace characters per line - Cucumber's
            // "smart" indentation convention; see HandleDocStringLine).
            // docStringLines accumulates each content line, RAW/un-stripped,
            // in source order, to be indent-stripped and newline-joined into
            // a single std::string on close.
            bool inDocString = false;
            std::size_t docStringIndent = 0;
            std::size_t docStringOpenLine = 0;
            std::vector<std::string> docStringLines;
            // INTERMEDIATE-recovery flag for a REJECTED Doc String open
            // ("doc string with no preceding step"/"step already has an
            // argument" - see HandleDocStringLine): unlike a rejected Data
            // Table (whose malformed rows are structurally distinguishable
            // from anything else, see skipMalformedTableLines above), a Doc
            // String's OPEN and CLOSE delimiter are the exact same three
            // characters - without this flag, the eventual closing '"""' of
            // a rejected attempt would be re-evaluated as a brand new
            // opening attempt and re-trigger the IDENTICAL error a second
            // time. While set, every line is silently swallowed (never
            // attached anywhere - the attempt was invalid) until either the
            // matching closing '"""' is found (clears the flag, no new
            // error) or a LooksLikeBlockBoundary line is reached first
            // (clears the flag and re-dispatches that line normally,
            // mirroring the genuinely-unclosed-Doc-String corruptive
            // recovery in ProcessFeatureLine - a rejected attempt must not
            // be allowed to silently swallow an entire subsequent Scenario
            // either).
            bool skipRejectedDocString = false;
        };

        // Every parse-error call site in this namespace funnels through
        // here instead of constructing/returning a ParseOutcome directly -
        // this is what lets ParseFeatureText keep processing the REST of
        // the text after one structural error instead of aborting (see
        // ParseOutcome's doc comment above). The message format
        // ("<line>: parse error: <text>") is deliberately NOT prefixed with
        // a feature label here - RunFeature() (which knows featureLabel)
        // prepends it once per accumulated error when relaying to
        // onFailure, giving a final "<file>:<line>: parse error: <message>"
        // one-liner without threading featureLabel down into the parser.
        inline void RecordParseError(FeatureParseState& state, std::size_t lineNo, std::string_view message) {
            state.errors.push_back(std::to_string(lineNo) + ": parse error: " + std::string(message));
        }

        // A line that starts a brand new top-level block, used by the
        // corruptive-error recovery below (see ProcessFeatureLine's
        // in-doc-string handling and HandleDocStringLine's rejected-attempt
        // skip) to resync instead of either (a) silently swallowing an
        // otherwise-valid Scenario/Background as if it were doc string
        // content, or (b) letting a rejected doc string's own "content"
        // lines misparse as real grammar. Deliberately NOT exhaustive of
        // every Gherkin construct (no Feature:/Examples:/Scenarios: /tags) -
        // just the handful of constructs that can legitimately follow a Doc
        // String block within the SAME Feature. Trade-off, accepted by
        // design: a genuine Doc String whose literal content happens to
        // contain a line starting with one of these keywords (e.g.
        // demonstrating Gherkin syntax inside documentation) is
        // misidentified as a resync boundary - an edge case judged far less
        // harmful than the alternative (silently losing an entire
        // subsequent Scenario's worth of structural errors to a single
        // unclosed/rejected Doc String).
        inline bool LooksLikeBlockBoundary(std::string_view trimmed) {
            return trimmed.starts_with("Scenario:") || trimmed.starts_with("Example:") ||
                   trimmed.starts_with("Scenario Outline:") || trimmed.starts_with("Scenario Template:") ||
                   trimmed.starts_with("Background:");
        }

        // Reject out-of-scope constructs (Rule, etc). Data Tables ('|' rows
        // outside an open Examples:/Scenarios: table) and Doc Strings
        // ('"""'-delimited blocks) are handled separately and BEFORE this
        // function is even reached - see HandleDataTableLine/
        // HandleDocStringLine and their call sites in ProcessFeatureLine.
        // POINT error (see RecordParseError/ParseFeatureText): self-
        // contained to this one line - the caller simply drops it and
        // continues with the next line, no resync needed.
        inline bool RejectIfUnsupportedConstruct(FeatureParseState& state, std::string_view trimmed,
                                                    std::size_t lineNo) {
            if (trimmed.starts_with("Rule:")) {
                RecordParseError(state, lineNo,
                                  "'Rule:' is not supported in this version (see docs/design/gherkin-support.md)");
                return true;
            }
            return false;
        }

        // Handle Feature: line; error if multiple Features. POINT error: a
        // second 'Feature:' line is dropped (the first Feature's name/tags
        // are kept), parsing continues normally from the next line.
        inline void HandleFeatureLine(FeatureParseState& state, std::string_view trimmed, std::size_t lineNo) {
            if (state.haveFeature) {
                RecordParseError(state, lineNo, "multiple 'Feature:' sections are not supported");
                return;
            }
            state.haveFeature = true;
            state.feature.name = std::string(TrimView(trimmed.substr(std::string_view("Feature:").size())));
            state.feature.tags = std::move(state.pendingTags);
            state.pendingTags.clear();
        }

        // Attach step to Background/Scenario; error if step before any
        // Background:/Scenario:. On success, attachedIndex is set to the
        // just-attached step's index into whichever container it landed in
        // (feature.background or currentScenario->steps) - returning it
        // this way (rather than making the caller re-derive it from
        // currentScenario->steps.size() after the fact) means the caller
        // never needs to dereference currentScenario itself, so there is no
        // unchecked-optional-access risk at the call site. Returns false
        // (POINT error - the step is dropped, parsing continues normally)
        // if there's nowhere to attach it.
        inline bool AttachParsedStep(FeatureParseState& state, bool inBackground, std::size_t lineNo, ParsedStep step,
                                       std::size_t& attachedIndex) {
            if (inBackground) {
                state.feature.background.push_back(std::move(step));
                attachedIndex = state.feature.background.size() - 1;
            } else if (state.currentScenario) {
                state.currentScenario->steps.push_back(std::move(step));
                attachedIndex = state.currentScenario->steps.size() - 1;
            } else {
                RecordParseError(state, lineNo, "step found outside of a Background:/Scenario:");
                return false;
            }
            return true;
        }

        // Resolve a StepTarget to the actual ParsedStep it refers to.
        // Precondition: target.inBackground == false implies
        // state.currentScenario is engaged - guaranteed by construction,
        // since lastStepTarget is always reset before currentScenario is
        // ever swapped/flushed (see ProcessFeatureLine). The explicit
        // has_value() check below is defensive (this should never actually
        // throw, given that precondition) but also satisfies static
        // analysis - state.currentScenario.value() with no antecedent check
        // is flagged by bugprone-unchecked-optional-access even though the
        // invariant genuinely holds.
        inline ParsedStep& ResolveStepTarget(FeatureParseState& state, const StepTarget& target) {
            if (target.inBackground) {
                return state.feature.background.at(target.index);
            }
            if (!state.currentScenario) {
                throw std::logic_error(
                    "BabyBehave::Gherkin: internal error - StepTarget refers to a Scenario step but no Scenario "
                    "is currently in progress");
            }
            return state.currentScenario->steps.at(target.index);
        }

        // Attach the in-progress Examples:/Scenarios: table (if any) to
        // state.currentScenario and reset all Outline/Examples parse state,
        // ready for whatever section comes next. MUST be called before every
        // place a scenario gets flushed (Background:, Scenario:/Example:/
        // Outline:/Template:, and end-of-file) - anything else risks
        // silently dropping a completed table or (worse) leaking it onto the
        // NEXT scenario. Records a parse error (POINT: self-contained to
        // this one Outline, the reset below always still runs so parser
        // state stays consistent for whatever follows) if the currently-
        // in-progress scenario was declared as a Scenario Outline/Template
        // but never got a usable Examples:/Scenarios: table (missing
        // entirely, or present but with zero data rows) - a silently-
        // vanishing scenario would be far more confusing than a hard error
        // here. On error, scenario.examples is deliberately left unset (the
        // scenario passes through ExpandScenarioOutlines unexpanded, as an
        // ordinary non-Outline Scenario) - harmless since a non-empty
        // errors vector already guarantees RunFeature() executes nothing.
        inline void FinalizeCurrentScenarioExamples(FeatureParseState& state) {
            if (state.currentScenarioIsOutline) {
                if (!state.pendingExamples) {
                    RecordParseError(state, state.currentScenario ? state.currentScenario->line : 0,
                        "'Scenario Outline:'/'Scenario Template:' has no 'Examples:'/'Scenarios:' table");
                } else if (state.pendingExamples->rows.empty()) {
                    RecordParseError(state, state.pendingExamples->headerLine,
                        "'Examples:'/'Scenarios:' table must have at least one data row");
                } else if (state.currentScenario) {
                    state.currentScenario->examples = std::move(state.pendingExamples);
                }
            }
            state.inExamplesTable = false;
            state.haveExamplesHeader = false;
            state.currentScenarioIsOutline = false;
            state.pendingExamples.reset();
        }

        // A '|' line reached OUTSIDE an open Examples:/Scenarios: table
        // (that context is handled directly in ProcessFeatureLine, before
        // this is ever called). Four cases:
        //   - state.inDataTable: the immediately-preceding line was itself
        //     a Data Table row for state.lastStepTarget - append another row,
        //     unless its cell count disagrees with the table's first
        //     (header) row, in which case - error (mirrors the Examples:
        //     table's header/row width check above). POINT error: the
        //     malformed row is simply dropped (not pushed), inDataTable
        //     stays true so any further, still-valid rows of this SAME
        //     table keep accumulating normally.
        //   - !state.lastStepTarget: no step immediately/recently attached
        //     (start of file, or right after Feature:/Background:/
        //     Scenario: with no step yet, or after a context change) -
        //     error. INTERMEDIATE recovery: sets skipMalformedTableLines so
        //     every remaining contiguous '|' row of this same stray table
        //     is silently skipped instead of re-reporting "no preceding
        //     step" once per row (see FeatureParseState's doc comment).
        //   - state.lastStepTarget's step already has a non-monostate
        //     rawArgument (a second '|' block after the first, once
        //     something other than a table row - e.g. a blank line - has
        //     reset state.inDataTable) - error "step already has an
        //     argument". Same INTERMEDIATE skip-remaining-rows recovery as
        //     above - a rejected second table's rows would otherwise
        //     re-trigger this identical error once per row too.
        //   - Otherwise: the FIRST row of a brand new Data Table for that
        //     step; attach it and start accumulating.
        inline void HandleDataTableLine(FeatureParseState& state, std::string_view trimmed, std::size_t lineNo) {
            const std::vector<std::string> cells = ParsePipeRow(trimmed);
            if (state.inDataTable) {
                if (!state.lastStepTarget) {
                    // Unreachable in practice: state.inDataTable is only
                    // ever set true right after state.lastStepTarget is
                    // set (see below), and both are cleared together on
                    // every context change. Kept so this never dereferences
                    // an empty optional.
                    RecordParseError(state, lineNo, "data table with no preceding step");
                    state.inDataTable = false;
                    state.skipMalformedTableLines = true;
                    return;
                }
                ParsedStep& step = ResolveStepTarget(state, *state.lastStepTarget);
                auto& table = std::get<DataTable>(step.rawArgument);
                if (!table.rows.empty() && cells.size() != table.rows.front().size()) {
                    RecordParseError(state, lineNo, "data table row has " + std::to_string(cells.size()) +
                                                         " cell(s), expected " +
                                                         std::to_string(table.rows.front().size()) +
                                                         " (from header)");
                    return;
                }
                table.rows.push_back(cells);
                return;
            }
            if (!state.lastStepTarget) {
                RecordParseError(state, lineNo, "data table with no preceding step");
                state.skipMalformedTableLines = true;
                return;
            }
            ParsedStep& step = ResolveStepTarget(state, *state.lastStepTarget);
            if (!std::holds_alternative<std::monostate>(step.rawArgument)) {
                RecordParseError(state, lineNo, "step already has an argument");
                state.skipMalformedTableLines = true;
                return;
            }
            step.rawArgument = DataTable{ .rows = { cells } };
            state.inDataTable = true;
        }

        // A '|' line reached WHILE an Examples:/Scenarios: table is open
        // (state.inExamplesTable - see ProcessFeatureLine, which routes
        // here BEFORE the Data Table '|' handling above; the two contexts
        // never both claim the same line). Either this block's own header
        // row (the first '|' line since the most recent Examples:/
        // Scenarios: line - state.haveExamplesHeader false) or a data row.
        //
        // Two different recovery strategies, deliberately NOT the same one:
        //   - A single bad DATA row (header already fine): POINT - record
        //     the error and STILL push the (malformed) cells into
        //     table.rows, then keep validating any further rows of this
        //     same block independently. Pushing the malformed row anyway
        //     (rather than dropping it) is what stops this from cascading
        //     into FinalizeCurrentScenarioExamples's separate "must have at
        //     least one data row" check if this happened to be the block's
        //     only row - a dropped row would make table.rows.empty() true
        //     and spuriously trigger a SECOND, unrelated-looking error for
        //     the exact same underlying mistake. This is safe precisely
        //     because ExpandScenarioOutlines is only ever invoked when
        //     state.errors is empty (see ParseFeatureText) - a malformed
        //     row is never actually substituted into a real scenario.
        //   - A bad HEADER on a second-or-later Examples:/Scenarios: block
        //     (its cell count doesn't match the count already established
        //     by an earlier block on the SAME Outline): INTERMEDIATE - sets
        //     skipMalformedTableLines so every remaining row of THIS one
        //     malformed block is silently skipped (see ProcessFeatureLine
        //     and FeatureParseState's doc comment), rather than validating
        //     each of them against a column count the user's own (bad)
        //     header didn't actually intend - a plain POINT/continue
        //     classification here would spuriously re-flag every one of
        //     that block's rows against the PRE-established header, one
        //     near-duplicate error per row, for what is really one mistake
        //     (the block's header line itself).
        //
        // Multiple Examples:/Scenarios: blocks on the same Scenario
        // Outline/Template are MERGED into a single state.pendingExamples
        // (see the 'Examples:'/'Scenarios:' line handling in
        // ProcessFeatureLine, which only resets state.haveExamplesHeader -
        // not state.pendingExamples itself - on a second-or-later block):
        // real Cucumber supports multiple named Examples: blocks per
        // outline, with every row from every block used, in declaration
        // order. A second-or-later block's own header row is therefore
        // validated against the FIRST block's column count (not
        // re-stored - the first block's column names are what <name>
        // substitution uses; see SubstitutePlaceholders).
        inline void HandleExamplesTableRow(FeatureParseState& state, std::string_view trimmed, std::size_t lineNo) {
            if (!state.pendingExamples) {
                // Unreachable in practice: state.inExamplesTable is only
                // ever set true together with state.pendingExamples being
                // engaged (see the 'Examples:'/'Scenarios:' line handling
                // in ProcessFeatureLine). Kept so this never dereferences
                // an empty optional.
                state.pendingExamples = ExamplesTable{};
            }
            ExamplesTable& table = *state.pendingExamples;
            const std::vector<std::string> cells = ParsePipeRow(trimmed);
            if (!state.haveExamplesHeader) {
                if (table.header.empty()) {
                    // The very first header this table has ever seen (first
                    // Examples:/Scenarios: block for this Outline).
                    table.header = cells;
                    table.headerLine = lineNo;
                } else if (cells.size() != table.header.size()) {
                    RecordParseError(state, lineNo, "Examples header has " + std::to_string(cells.size()) +
                                                         " cell(s) but an earlier 'Examples:'/'Scenarios:' block on "
                                                         "this 'Scenario Outline:'/'Scenario Template:' declared " +
                                                         std::to_string(table.header.size()));
                    // INTERMEDIATE recovery: see function comment above.
                    state.skipMalformedTableLines = true;
                    return;
                }
                // A later block's header matching column count is accepted
                // without overwriting table.header - see function comment.
                state.haveExamplesHeader = true;
                return;
            }
            if (cells.size() != table.header.size()) {
                RecordParseError(state, lineNo, "Examples row has " + std::to_string(cells.size()) +
                                                     " cell(s) but the header declares " +
                                                     std::to_string(table.header.size()));
                // POINT recovery: still push the malformed row (see function
                // comment above for why - avoids a spurious cascade into
                // FinalizeCurrentScenarioExamples's separate "must have at
                // least one data row" check). Never actually substituted
                // into a real scenario: ExpandScenarioOutlines only runs
                // when state.errors is empty (see ParseFeatureText).
                table.rows.push_back(ExamplesRow{ .line = lineNo, .cells = cells });
                return;
            }
            table.rows.push_back(ExamplesRow{ .line = lineNo, .cells = cells });
        }

        // The Doc String delimiter (see docs/design/gherkin-support.md).
        // Cucumber's alternate ''' delimiter is intentionally NOT supported
        // (judged out of scope for this version): """ alone covers the
        // common case, and restricting to one delimiter keeps open/close
        // detection unambiguous (no line any other construct recognizes -
        // tag, comment, table row, keyword - can ever start with a quote
        // character, so there is no ordering hazard in ProcessFeatureLine).
        inline constexpr std::string_view kDocStringDelimiter = R"(""")";

        // Strip up to `indent` leading whitespace characters (spaces/tabs)
        // from `line` - Cucumber's "smart" Doc String indentation
        // convention: the OPENING '"""' marker's own column is stripped
        // from every content line, INDIVIDUALLY. A content line with LESS
        // leading whitespace than `indent` has only its own leading
        // whitespace stripped (never more, never goes negative) - this
        // deliberately tolerates inconsistently-indented content rather
        // than erroring.
        inline std::string StripDocStringIndent(std::string_view line, std::size_t indent) {
            std::size_t stripped = 0;
            while (stripped < indent && stripped < line.size() &&
                   (line[stripped] == ' ' || line[stripped] == '\t')) {
                ++stripped;
            }
            return std::string(line.substr(stripped));
        }

        // A '"""' Doc String line - either the OPENING delimiter for a new
        // block, a literal CONTENT line inside an already-open block, or
        // the CLOSING delimiter for one. Called from ProcessFeatureLine in
        // two situations: state.inDocString (content/closing line - every
        // such line is routed here BEFORE any other classification, so
        // literal content is never misparsed as a tag/comment/table row -
        // see ProcessFeatureLine's inDocString short-circuit), or a line
        // that starts with kDocStringDelimiter while !state.inDocString
        // (candidate opening line). Mirrors HandleDataTableLine's
        // attach-once contract for the opening case:
        //   - !state.lastStepTarget: no step immediately/recently attached -
        //     error "doc string with no preceding step".
        //   - state.lastStepTarget's step already has a non-monostate
        //     rawArgument (a Data Table already attached, or this is a
        //     second Doc String) - error "step already has an argument",
        //     same wording Data Tables use - a step cannot have both, or
        //     two of either kind; whichever is attached first wins the slot.
        //   - Otherwise: record this (opening) line's own leading-
        //     whitespace column as the indent to strip on close, and start
        //     accumulating content lines verbatim.
        // For an already-open block: the closing '"""' line indent-strips
        // (see StripDocStringIndent) and newline-joins every accumulated
        // content line (preserving blank lines in between) into a single
        // std::string, attaches it to the target step's rawArgument, and
        // closes the block; any other line is appended to
        // state.docStringLines RAW/verbatim, with zero interpretation.
        // Errors here are POINT ("doc string with no preceding step"/"step
        // already has an argument" - a single mistaken second table/doc-
        // string attachment - see FeatureParseState::skipRejectedDocString's
        // doc comment for why the OPENING error path still needs to swallow
        // lines up to the matching closing '"""': unlike a Data Table's
        // first-bad-row (structurally distinct from a valid row), a Doc
        // String's own closing delimiter is byte-identical to its opening
        // one, so without swallowing, that closing line would be re-
        // evaluated as a brand new (and identically invalid) opening
        // attempt, reporting the SAME mistake twice for one typo).
        inline void HandleDocStringLine(FeatureParseState& state, std::string_view raw, std::string_view trimmed,
                                           std::size_t lineNo) {
            if (state.inDocString) {
                if (trimmed.starts_with(kDocStringDelimiter)) {
                    std::string joined;
                    for (std::size_t i = 0; i < state.docStringLines.size(); ++i) {
                        if (i > 0) {
                            joined += '\n';
                        }
                        joined += StripDocStringIndent(state.docStringLines[i], state.docStringIndent);
                    }
                    if (!state.lastStepTarget) {
                        // Unreachable in practice: reaching a CLOSING '"""'
                        // with state.inDocString true implies the OPENING
                        // '"""' already validated state.lastStepTarget below
                        // (that is the only place state.inDocString is ever
                        // set true). Kept so this never dereferences an
                        // empty optional.
                        RecordParseError(state, lineNo, "doc string with no preceding step");
                        state.inDocString = false;
                        state.docStringLines.clear();
                        return;
                    }
                    ParsedStep& step = ResolveStepTarget(state, *state.lastStepTarget);
                    step.rawArgument = std::move(joined);
                    state.inDocString = false;
                    state.docStringLines.clear();
                    return;
                }
                state.docStringLines.emplace_back(raw);
                return;
            }
            if (!state.lastStepTarget) {
                RecordParseError(state, lineNo, "doc string with no preceding step");
                state.skipRejectedDocString = true;
                return;
            }
            ParsedStep& step = ResolveStepTarget(state, *state.lastStepTarget);
            if (!std::holds_alternative<std::monostate>(step.rawArgument)) {
                RecordParseError(state, lineNo, "step already has an argument");
                state.skipRejectedDocString = true;
                return;
            }
            state.docStringIndent = LeadingWhitespaceCount(raw);
            state.docStringOpenLine = lineNo;
            state.docStringLines.clear();
            state.inDocString = true;
        }

        // Handle a Scenario:/Example:/Scenario Outline:/Scenario Template:
        // line - flushes whatever Scenario was previously in progress
        // (finalizing its Examples: table first if it was an Outline/
        // Template - see FinalizeCurrentScenarioExamples) and starts a new
        // one. Split out of ProcessFeatureLine purely to keep that
        // function's cognitive complexity under this codebase's clang-tidy
        // threshold.
        inline void HandleScenarioHeaderLine(FeatureParseState& state, std::string_view trimmed, std::size_t lineNo) {
            state.lastStepTarget.reset();
            FinalizeCurrentScenarioExamples(state);
            FlushScenario(state.feature, state.currentScenario);
            state.inBackground = false;
            state.currentScenarioIsOutline =
                trimmed.starts_with("Scenario Outline:") || trimmed.starts_with("Scenario Template:");
            const std::size_t colonPos = trimmed.find(':');
            ParsedScenario scenario;
            scenario.name = std::string(TrimView(trimmed.substr(colonPos + 1)));
            scenario.tags = std::move(state.pendingTags);
            state.pendingTags.clear();
            scenario.line = lineNo;
            state.currentScenario = std::move(scenario);
        }

        // Handle a step-keyword (Given/When/Then/And/But/...) line - attaches
        // the step to whichever container (Background vs the in-progress
        // Scenario) is currently active, or rejects it with a clear parse
        // error if it appears after this Outline's own Examples:/Scenarios:
        // table (Bug 2 fix - Examples:/Scenarios: always comes last in a
        // Scenario Outline/Template; see FeatureParseState::inExamplesTable),
        // or before any Background:/Scenario: at all (see AttachParsedStep).
        // Both errors are POINT: the step line is dropped, parsing continues
        // normally with the next line. Split out of ProcessFeatureLine for
        // the same cognitive-complexity reason as HandleScenarioHeaderLine above.
        inline void HandleStepKeywordLine(FeatureParseState& state, std::string_view raw,
                                             std::pair<StepKeyword, std::string_view> matched, std::size_t lineNo) {
            if (state.inExamplesTable) {
                RecordParseError(state, lineNo,
                    "step keyword found after this 'Scenario Outline:'/'Scenario Template:''s "
                    "'Examples:'/'Scenarios:' table (steps must be declared before Examples:/Scenarios:)");
                // INTERMEDIATE recovery: state.inExamplesTable stays true (it
                // is only cleared at Finalize/next block), so any pipe lines
                // immediately following this rejected step would otherwise be
                // misrouted into HandleExamplesTableRow - either silently
                // accepted as bogus extra data rows (if their cell count
                // happens to match the real header) or spamming a fresh
                // "Examples row has N cell(s) but header declares M" error
                // per line (if it doesn't). Neither outcome is acceptable, so
                // swallow the rest of this contiguous pipe-line region via
                // the same mechanism used for other malformed tables.
                state.skipMalformedTableLines = true;
                return;
            }
            const auto& [keyword, rest] = matched;
            ParsedStep step;
            step.keyword = keyword;
            step.text = std::string(TrimView(rest));
            step.line = lineNo;
            step.column = LeadingWhitespaceCount(raw) + 1;
            const bool inBackground = state.inBackground;
            std::size_t attachedIndex = 0;
            if (AttachParsedStep(state, inBackground, lineNo, std::move(step), attachedIndex)) {
                state.lastStepTarget = StepTarget{ .inBackground = inBackground, .index = attachedIndex };
            } else {
                state.lastStepTarget.reset();
            }
        }

        // Handle an 'Examples:'/'Scenarios:' header line for a Scenario
        // Outline/Template. INTERMEDIATE recovery for the "no preceding
        // Outline" case: a stray Examples:/Scenarios: block is exactly as
        // prone to per-row duplicate spam as a malformed Data Table (its
        // rows would otherwise misparse as Data Table rows, one "no
        // preceding step"/"already has an argument" error per row) - reuses
        // the same skipMalformedTableLines mechanism. Otherwise starts (or
        // merges into, for a second/later Examples:/Scenarios: block on the
        // same Outline - see HandleExamplesTableRow) the pending table. Also
        // split out of ProcessFeatureLine for the same cognitive-complexity
        // reason as HandleScenarioHeaderLine/HandleStepKeywordLine above -
        // but unlike HandleScenarioHeaderLine, this function is not a purely
        // mechanical extraction: the skipMalformedTableLines recovery for a
        // stray Examples:/Scenarios: header is real logic that lives here.
        inline void HandleExamplesHeaderLine(FeatureParseState& state, std::size_t lineNo) {
            state.lastStepTarget.reset();
            if (!state.currentScenarioIsOutline) {
                RecordParseError(state, lineNo,
                    "'Examples:'/'Scenarios:' without a preceding 'Scenario Outline:'/'Scenario Template:'");
                state.skipMalformedTableLines = true;
                return;
            }
            state.inExamplesTable = true;
            state.haveExamplesHeader = false;
            // A second (or later) Examples:/Scenarios: block on the same
            // Outline MERGES into the already-in-progress table (see
            // HandleExamplesTableRow) rather than starting a fresh one - real
            // Cucumber supports multiple named Examples: blocks per outline,
            // with every row from every block used, in declaration order.
            // Only start a brand new table when this is genuinely the first
            // block for this Outline (nothing collected yet).
            if (!state.pendingExamples || state.pendingExamples->rows.empty()) {
                state.pendingExamples = ExamplesTable{};
            }
        }

        // INTERMEDIATE recovery for a REJECTED Doc String open attempt (see
        // FeatureParseState::skipRejectedDocString's doc comment): swallows
        // every line up to and including its own matching closing '"""'
        // (never attached anywhere - the attempt was invalid), UNLESS a
        // block boundary is reached first (mirrors the corruptive resync in
        // ConsumeDocStringLine below for a genuinely-unclosed Doc String - a
        // rejected attempt must not be allowed to silently eat an entire
        // subsequent Scenario). Returns true if this line was fully consumed
        // (caller must return without further processing); returns false if
        // there was nothing to skip, OR if a block boundary was just reached
        // and skipRejectedDocString was cleared - in the latter case the
        // caller must fall through and re-dispatch this SAME line normally,
        // it is NOT yet consumed. Also split out of ProcessFeatureLine to
        // keep that function's cognitive complexity manageable, but this is
        // not a mechanical extraction of pre-existing code: the corruptive
        // resync it performs (swallow-until-close-or-boundary for a rejected
        // Doc String attempt) is real recovery logic that has no other
        // caller.
        inline bool ConsumeSkippedRejectedDocStringLine(FeatureParseState& state, std::string_view trimmed) {
            if (!state.skipRejectedDocString) {
                return false;
            }
            if (trimmed.starts_with(kDocStringDelimiter)) {
                state.skipRejectedDocString = false;
                return true;
            }
            if (LooksLikeBlockBoundary(trimmed)) {
                state.skipRejectedDocString = false;
                return false; // Fall through: reprocess this boundary line normally.
            }
            return true;
        }

        // Doc String content/closing mode: an open '"""' block claims EVERY
        // line until its closing '"""', bypassing every other classification
        // in ProcessFeatureLine (blank/comment/tag/table/keyword/...) - see
        // FeatureParseState::inDocString and HandleDocStringLine. CORRUPTIVE
        // recovery: if a block-boundary-shaped line (see
        // LooksLikeBlockBoundary) is reached BEFORE any closing '"""', the
        // Doc String is treated as unclosed right here (instead of only
        // discovering this once EOF is reached) - the error is recorded
        // against the OPENING line. Without this, a single unclosed Doc
        // String earlier in the file would silently eat every subsequent
        // Scenario's text as "content", hiding their own independent
        // structural errors entirely (see docs/design/gherkin-support.md and
        // this feature's own design notes on why that would violate "report
        // everything found in one pass"). Returns true if this line was
        // fully consumed (caller must return); returns false if inDocString
        // was already clear, OR if an unclosed Doc String was just detected
        // and recovered - in the latter case the caller must fall through
        // and re-dispatch this SAME boundary line normally. Also split out
        // of ProcessFeatureLine to keep that function's cognitive complexity
        // manageable, but this is not a mechanical extraction of
        // pre-existing code: the corruptive early-detection of an unclosed
        // Doc String at a block boundary (rather than only at EOF) is real
        // recovery logic that has no other caller.
        inline bool ConsumeDocStringLine(FeatureParseState& state, std::string_view raw, std::string_view trimmed,
                                          std::size_t lineNo) {
            if (!state.inDocString) {
                return false;
            }
            if (!trimmed.starts_with(kDocStringDelimiter) && LooksLikeBlockBoundary(trimmed)) {
                RecordParseError(state, state.docStringOpenLine,
                                  R"(doc string is not closed (missing terminating '"""'))");
                state.inDocString = false;
                state.docStringLines.clear();
                return false; // Fall through: reprocess this boundary line normally.
            }
            HandleDocStringLine(state, raw, trimmed, lineNo);
            return true;
        }

        // Classify trimmed Gherkin line; record an error (see
        // RecordParseError) or do nothing (including for free-text
        // description lines) - never aborts the parse. Per-line decision
        // tree. See FeatureParseState's doc comments (skipMalformedTableLines/
        // skipRejectedDocString) and ParseOutcome's doc comment above for the
        // overall point/intermediate/corruptive recovery-strategy rationale.
        inline void ProcessFeatureLine(FeatureParseState& state, std::string_view raw, std::size_t lineNo) {
            const std::string_view trimmed = TrimView(raw);

            if (ConsumeSkippedRejectedDocStringLine(state, trimmed)) {
                return;
            }
            if (ConsumeDocStringLine(state, raw, trimmed, lineNo)) {
                return;
            }
            // A Data Table only continues across CONSECUTIVE '|' rows - any
            // other kind of line (blank, comment, tag, a new construct, ...)
            // ends the accumulation, so the NEXT '|' line (if any) is
            // re-evaluated from scratch by HandleDataTableLine (either as a
            // fresh table for a still-eligible step, or as a "step already
            // has an argument" error - see FeatureParseState::inDataTable).
            const bool isPipeLine = !trimmed.empty() && trimmed.front() == '|';
            if (!isPipeLine) {
                state.inDataTable = false;
                state.skipMalformedTableLines = false;
            }
            if (trimmed.empty() || trimmed.front() == '#') {
                return;
            }
            if (trimmed.front() == '@') {
                AppendTagsFromLine(trimmed, state.pendingTags);
                return;
            }
            // INTERMEDIATE recovery continuation: still swallowing the
            // remaining rows of an already-reported malformed table (see
            // FeatureParseState::skipMalformedTableLines) - deliberately
            // checked regardless of state.inExamplesTable, since this flag
            // is set from BOTH contexts (a malformed Data Table outside any
            // Examples: block, and a malformed second Examples:/Scenarios:
            // header WHILE inExamplesTable is still true - see
            // HandleExamplesTableRow) and is only ever set from whichever
            // one is actually active, never both at once.
            if (isPipeLine && state.skipMalformedTableLines) {
                return;
            }
            // Must run BEFORE the Data Table '|' handling below: a bare '|'
            // line is an Examples row while an Examples:/Scenarios: table is
            // actually open, and a Data Table row otherwise - two distinct
            // contexts that must never both claim the same line.
            if (state.inExamplesTable && trimmed.front() == '|') {
                HandleExamplesTableRow(state, trimmed, lineNo);
                return;
            }
            if (isPipeLine) {
                HandleDataTableLine(state, trimmed, lineNo);
                return;
            }
            if (trimmed.starts_with(kDocStringDelimiter)) {
                HandleDocStringLine(state, raw, trimmed, lineNo);
                return;
            }
            if (RejectIfUnsupportedConstruct(state, trimmed, lineNo)) {
                return;
            }
            if (trimmed.starts_with("Feature:")) {
                state.lastStepTarget.reset();
                HandleFeatureLine(state, trimmed, lineNo);
                return;
            }
            if (trimmed.starts_with("Background:")) {
                state.lastStepTarget.reset();
                FinalizeCurrentScenarioExamples(state);
                FlushScenario(state.feature, state.currentScenario);
                state.inBackground = true;
                state.pendingTags.clear(); // Background: does not take tags in this version
                return;
            }
            if (trimmed.starts_with("Scenario:") || trimmed.starts_with("Example:") ||
                trimmed.starts_with("Scenario Outline:") || trimmed.starts_with("Scenario Template:")) {
                HandleScenarioHeaderLine(state, trimmed, lineNo);
                return;
            }
            if (trimmed.starts_with("Examples:") || trimmed.starts_with("Scenarios:")) {
                HandleExamplesHeaderLine(state, lineNo);
                return;
            }
            if (const auto matched = MatchStepKeyword(trimmed)) {
                HandleStepKeywordLine(state, raw, *matched, lineNo);
                return;
            }
            // Ignorable free-text description line.
        }

        // Replace every <name> token in `text` with the matching Examples
        // column's cell for this row; a <name> with no matching header
        // column is left as literal text (surfaces later as an ordinary "no
        // step definition matches" failure - no extra validation needed
        // here). Runs on ParsedStep::text (a plain literal at parse time),
        // entirely independent of CompileStepPattern (which only compiles a
        // step DEFINITION's cucumber-expression pattern at StepRegistry
        // registration time) - two different strings, two different
        // pipeline stages, no collision with {int}-style placeholders.
        inline std::string SubstitutePlaceholders(std::string_view text, const std::vector<std::string>& header,
                                                    const std::vector<std::string>& cells) {
            std::string out;
            out.reserve(text.size());
            std::size_t index = 0;
            while (index < text.size()) {
                if (text[index] != '<') {
                    out += text[index];
                    ++index;
                    continue;
                }
                const std::size_t close = text.find('>', index);
                if (close == std::string_view::npos) {
                    out += text.substr(index);
                    break;
                }
                const std::string name(text.substr(index + 1, close - index - 1));
                const auto headerIt = std::ranges::find(header, name);
                if (headerIt != header.end()) {
                    const auto columnIndex = static_cast<std::size_t>(std::distance(header.begin(), headerIt));
                    out += cells.at(columnIndex);
                } else {
                    out += text.substr(index, close - index + 1); // no matching column: left literal
                }
                index = close + 1;
            }
            return out;
        }

        // Expand every Scenario Outline/Template (i.e. every ParsedScenario
        // with scenario.examples set - see FinalizeCurrentScenarioExamples)
        // into one ordinary, independent ParsedScenario per Examples data
        // row, replacing it in feature.scenarios in place; a plain Scenario
        // (examples == nullopt) passes through unchanged. Declaration order
        // is preserved across the whole feature - each outline's rows
        // in order, then whatever follows it - and expanded rows get
        // examples == nullopt themselves, making them indistinguishable
        // from hand-written Scenarios to every caller downstream of parsing
        // (RunFeature/RunScenario need zero changes for this feature, and
        // later features like timeout/retry/parallel never need to special-
        // case "was this an outline row").
        //
        // Every failure mode (missing Examples:, header-only Examples:, a
        // row/header cell-count mismatch) is already rejected earlier at
        // parse time (see FinalizeCurrentScenarioExamples and the '|' row
        // handling in ProcessFeatureLine), so this function cannot fail and
        // has no error to report - unlike ParseFeatureText's other,
        // error-propagating helpers.
        inline void ExpandScenarioOutlines(ParsedFeature& feature) {
            std::vector<ParsedScenario> expanded;
            expanded.reserve(feature.scenarios.size());
            for (auto& scenario : feature.scenarios) {
                if (!scenario.examples) {
                    expanded.push_back(std::move(scenario));
                    continue;
                }
                const ExamplesTable& table = *scenario.examples;
                for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex) {
                    const ExamplesRow& row = table.rows[rowIndex];
                    ParsedScenario rowScenario;
                    rowScenario.name = scenario.name + " (Examples row " + std::to_string(rowIndex + 1) + ")";
                    rowScenario.tags = scenario.tags;
                    // The ROW's own source line (best for "which data failed").
                    rowScenario.line = row.line;
                    rowScenario.steps.reserve(scenario.steps.size());
                    for (const auto& templateStep : scenario.steps) {
                        ParsedStep rowStep;
                        rowStep.keyword = templateStep.keyword;
                        rowStep.text = SubstitutePlaceholders(templateStep.text, table.header, row.cells);
                        // The TEMPLATE step's own line/column (best for
                        // "which step pattern is wrong") - deliberately NOT
                        // the row's line, unlike rowScenario.line above.
                        rowStep.line = templateStep.line;
                        rowStep.column = templateStep.column;
                        // Copied verbatim (no <name> substitution inside a
                        // table's cells) - a Data Table attached to an
                        // Outline template step is replicated to every
                        // expanded row's copy of that step as-is.
                        rowStep.rawArgument = templateStep.rawArgument;
                        rowScenario.steps.push_back(std::move(rowStep));
                    }
                    // rowScenario.examples left at nullopt: see function comment.
                    expanded.push_back(std::move(rowScenario));
                }
            }
            feature.scenarios = std::move(expanded);
        }

        // Free-text prose under Feature:/Scenario:/Background: ignored (no
        // executable meaning). Parses the ENTIRE text unconditionally - one
        // structural error no longer aborts the parse, it is recorded (see
        // RecordParseError) and parsing continues, so a single pass over a
        // malformed .feature file surfaces every distinct structural error
        // it contains, not just the first. outcome.ok reflects whether
        // state.errors ended up empty; RunFeature() enforces the
        // conservative invariant that a non-empty errors vector means ZERO
        // Scenarios ever execute, regardless of how much of the file parsed
        // "successfully" around the error(s) - see RunFeature's own doc
        // comment. outcome.feature is populated either way (useful for
        // impl::-level parser tests inspecting what recovery did manage to
        // capture), never consulted for execution when !outcome.ok.
        inline ParseOutcome ParseFeatureText(std::string_view text) {
            const std::vector<std::string_view> lines = SplitLines(text);
            FeatureParseState state;

            for (std::size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
                ProcessFeatureLine(state, lines[lineIdx], lineIdx + 1);
            }
            // A Doc String still open at end-of-file (opening '"""' with no
            // matching closing '"""' and no block boundary reached first
            // either - see ProcessFeatureLine's inDocString corruptive-
            // resync handling above) would otherwise silently drop its
            // accumulated content (never attached to the step, since that
            // only happens on close) - report it against the OPENING line,
            // matching RecordParseError's "point at the line the user needs
            // to fix" convention used everywhere else in this parser.
            if (state.inDocString) {
                RecordParseError(state, state.docStringOpenLine,
                                  R"(doc string is not closed (missing terminating '"""'))");
            }
            FinalizeCurrentScenarioExamples(state);
            FlushScenario(state.feature, state.currentScenario);
            if (!state.haveFeature) {
                RecordParseError(state, 0, "no 'Feature:' found");
            }
            const bool ok = state.errors.empty();
            // Only expand Scenario Outlines when the parse is clean.
            // Several recovery paths above (see HandleExamplesTableRow)
            // deliberately leave a malformed Examples: row/table in a
            // "present but structurally inconsistent" state on purpose
            // (pushed anyway, to avoid a spurious secondary "must have at
            // least one data row" cascade) - safe ONLY as long as
            // SubstitutePlaceholders/ExpandScenarioOutlines never actually
            // indexes into it, which is exactly what skipping expansion
            // here guarantees. outcome.feature therefore stays in its
            // un-expanded, raw-Outline-with-Examples-table shape whenever
            // !ok - fine for impl::-level parser tests inspecting recovery,
            // and irrelevant for execution (RunFeature() never touches
            // parsed.feature when !parsed.ok - see RunFeature's own doc
            // comment on the conservative zero-scenarios invariant).
            if (ok) {
                ExpandScenarioOutlines(state.feature);
            }
            ParseOutcome outcome;
            outcome.ok = ok;
            outcome.errors = std::move(state.errors);
            outcome.feature = std::move(state.feature);
            return outcome;
        }

        inline std::string MakeFeatureLocation(std::string_view featureLabel, std::size_t line, std::size_t column) {
            return std::string(featureLabel) + ":" + std::to_string(line) + ":" + std::to_string(column);
        }

    } // namespace impl

    // Public alias of the existing internal keyword enum (impl::StepKeyword
    // above) - deliberately NOT a second, duplicate enum: RegisterStep/
    // RegisterSteps/StepEntry below need a public-facing name for the same
    // five keywords already used internally, and aliasing avoids two
    // enums drifting out of sync.
    using Keyword = impl::StepKeyword;

    // One step registration's worth of data, for bulk registration via
    // StepRegistry::RegisterSteps() below. Named StepEntry (not Step) to
    // avoid reader confusion with the pre-existing, unrelated
    // BabyBehaveTest::Step. Relies on C++20 aggregate CTAD to deduce F from
    // the initializer, e.g.: StepEntry{Keyword::Given, "a thing", fn}.
    template<typename F>
    struct StepEntry {
        Keyword keyword = Keyword::Given;
        std::string pattern;
        F fn;
    };

    // Consumer-constructed registry of step definitions (Given/When/Then/And/But
    // with {int}/{float}/{string}/{word} placeholders), per-Scenario
    // Before/After hooks (tag-filtered AND/subset), and Suite-level
    // Before-ALL/After-ALL hooks (unconditional, run once per RunFeature()
    // call - see AddBeforeAllHook/AddAfterAllHook below). NOT a singleton;
    // passed by reference to RunFeature.
    class StepRegistry {
    public:
        // Explicitly defaulted: documents that copy/move are an intentional,
        // supported contract (every member - impl::StepDefinition's
        // std::function thunk, impl::Hook - is already copyable), not an
        // accident that a future move-only member addition could silently
        // break. Behaviorally identical to the implicit special members this
        // class already had (no user-declared constructor/destructor
        // previously existed), so this is documentation, not a functional change.
        StepRegistry() = default;
        ~StepRegistry() = default;
        StepRegistry(const StepRegistry&) = default;
        StepRegistry& operator=(const StepRegistry&) = default;
        StepRegistry(StepRegistry&&) = default;
        StepRegistry& operator=(StepRegistry&&) = default;

        // Register step definitions. Named RegisterGiven/.../RegisterBut (not
        // Given/When/etc) to avoid macro collision with fluent DSL macros.
        template<typename F>
        void RegisterGiven(std::string pattern, F stepFn) {
            AddStepDefinition(impl::StepKeyword::Given, std::move(pattern), std::move(stepFn));
        }
        template<typename F>
        void RegisterWhen(std::string pattern, F stepFn) {
            AddStepDefinition(impl::StepKeyword::When, std::move(pattern), std::move(stepFn));
        }
        template<typename F>
        void RegisterThen(std::string pattern, F stepFn) {
            AddStepDefinition(impl::StepKeyword::Then, std::move(pattern), std::move(stepFn));
        }
        template<typename F>
        void RegisterAnd(std::string pattern, F stepFn) {
            AddStepDefinition(impl::StepKeyword::And, std::move(pattern), std::move(stepFn));
        }
        template<typename F>
        void RegisterBut(std::string pattern, F stepFn) {
            AddStepDefinition(impl::StepKeyword::But, std::move(pattern), std::move(stepFn));
        }

        // Registers the SAME pattern+callable under several keywords at
        // once, e.g. an idempotent assertion reachable via both Then and
        // And. F must be copyable (a fresh copy is registered per keyword -
        // see the loop below; use RegisterSteps() instead if fn is move-only).
        // CRITICAL: fn itself is never moved into the loop - each iteration
        // copies it before dispatching, since the underlying Register*
        // path consumes its callable by value + internal std::move; moving
        // fn directly would leave every keyword after the first with a
        // moved-from callable.
        template<typename F>
        void RegisterStep(std::initializer_list<Keyword> keywords, std::string pattern, F stepFn) {
            for (const Keyword keyword : keywords) {
                F fnCopy = stepFn; // Fresh copy per keyword - see doc comment above.
                switch (keyword) {
                    case Keyword::Given: RegisterGiven(pattern, std::move(fnCopy)); break;
                    case Keyword::When:  RegisterWhen(pattern, std::move(fnCopy)); break;
                    case Keyword::Then:  RegisterThen(pattern, std::move(fnCopy)); break;
                    case Keyword::And:   RegisterAnd(pattern, std::move(fnCopy)); break;
                    case Keyword::But:   RegisterBut(pattern, std::move(fnCopy)); break;
                }
            }
        }

        // Variadic bulk registration: one call registers any number of
        // StepEntry<F> values, each with its own (possibly different) F,
        // e.g.: registry.RegisterSteps(StepEntry{Keyword::Given, "...", fn1},
        // StepEntry{Keyword::When, "...", fn2}). Deliberately a variadic
        // PARAMETER PACK, not std::initializer_list<StepEntry<...>>:
        // initializer_list only ever exposes `const T&`, which is
        // incompatible with StepFunction being a std::move_only_function
        // on the C++23-and-later path - a move-only callable inside a
        // StepEntry could never be moved out of a const initializer_list
        // element. The parameter pack forwards each entry individually
        // instead, with no intermediate array and no copyability requirement,
        // so a move-only lambda (e.g. one capturing a std::unique_ptr)
        // works here even though it does NOT work with RegisterStep() above.
        template<typename... Fs>
        void RegisterSteps(StepEntry<Fs>&&... entries) {
            (RegisterOneStep(std::move(entries)), ...);
        }

        // Hook registration (tag-filtered AND/subset). Empty tag list means always run.
        // Before hooks run in registration order before Background.
        // After hooks run in registration order (not reversed) after steps.
        void AddBeforeHook(std::vector<std::string> tags, HookFunction hookFn) {
            const std::string label = impl::JoinTagsForLabel(tags);
            m_beforeHooks.push_back(impl::Hook{ .tags = std::move(tags), .fn = std::move(hookFn), .label = label });
        }
        void AddAfterHook(std::vector<std::string> tags, HookFunction hookFn) {
            const std::string label = impl::JoinTagsForLabel(tags);
            m_afterHooks.push_back(impl::Hook{ .tags = std::move(tags), .fn = std::move(hookFn), .label = label });
        }

        // Hook registration (tag-filtered by a boolean AND/OR/NOT EXPRESSION,
        // e.g. "@smoke and not @slow" - see impl::ParseTagExpression for the
        // full grammar). A parallel, more expressive alternative to
        // AddBeforeHook/AddAfterHook above; deliberately NOT an overload of
        // those (a same-named std::string overload would be ambiguous - or
        // silently wrong - against existing `AddBeforeHook({}, fn)` call
        // sites, since `{}` could resolve to either). `expression` is parsed
        // EAGERLY, right here at registration time: throws
        // std::invalid_argument immediately on any malformed expression
        // (mirrors CompileStepPattern's registration-time throw style),
        // never later during RunFeature. Before/After ordering and
        // Background/timing semantics are otherwise identical to
        // AddBeforeHook/AddAfterHook.
        void AddBeforeHookExpr(const std::string& expression, HookFunction hookFn) {
            std::shared_ptr<impl::TagExpressionNode> parsed = impl::ParseTagExpression(expression);
            m_beforeHooks.push_back(impl::Hook{
                .tags = {}, .fn = std::move(hookFn), .label = expression, .expression = std::move(parsed) });
        }
        void AddAfterHookExpr(const std::string& expression, HookFunction hookFn) {
            std::shared_ptr<impl::TagExpressionNode> parsed = impl::ParseTagExpression(expression);
            m_afterHooks.push_back(impl::Hook{
                .tags = {}, .fn = std::move(hookFn), .label = expression, .expression = std::move(parsed) });
        }

        // Sugar pairing a Before+After hook under the same tag filter in
        // one call (e.g. timing/logging bracketing a set of scenarios).
        // Forwards directly to AddBeforeHook+AddAfterHook - no duplicated
        // dispatch logic, so `before`/`after` share the exact same
        // registration-order/tag-matching semantics as registering them
        // separately would.
        void AddAroundHook(std::vector<std::string> tags, HookFunction before, HookFunction after) {
            AddBeforeHook(tags, std::move(before));
            AddAfterHook(std::move(tags), std::move(after));
        }

        // Expression-based variant of AddAroundHook() above - forwards to
        // AddBeforeHookExpr+AddAfterHookExpr (see those for the full
        // expression grammar and registration-time fail-fast behavior).
        void AddAroundHookExpr(const std::string& expression, HookFunction before, HookFunction after) {
            AddBeforeHookExpr(expression, std::move(before));
            AddAfterHookExpr(expression, std::move(after));
        }

        // Suite-level hook registration (Feature 8). Unlike AddBeforeHook/
        // AddAfterHook above, these are NEVER tag-filtered and run exactly
        // ONCE per RunFeature() call - not once per matching Scenario. A
        // Before-ALL hook runs once, before ANY Scenario in the Feature
        // starts (before the Scenario-dispatch loop, in both serial and
        // enableParallelScenarios==true mode); an After-ALL hook runs once,
        // after EVERY Scenario has finished (after the parallel dispatch's
        // futures are all collected, in parallel mode - never overlapping
        // with in-flight Scenario execution). Multiple Before-ALL (resp.
        // After-ALL) hooks run in registration order, mirroring
        // AddBeforeHook/AddAfterHook's own "registration order, not
        // reversed" convention above.
        //
        // *** SAFETY CAVEAT (mirrors enableParallelScenarios' onFailure
        // warning above) ***: After-ALL hooks are only GUARANTEED to run if
        // Scenario failures don't cause RunFeature to exit/abort early. The
        // DEFAULT onFailure (impl::DefaultGherkinFailureAction) calls
        // std::exit() on the FIRST failing Scenario - so under the default
        // callback, if ANY Scenario fails, the process exits immediately and
        // registered After-ALL hooks NEVER run (same fail-hard-by-default
        // philosophy as everywhere else in this file). An After-ALL hook
        // used for suite-wide cleanup that MUST run even when a Scenario
        // fails requires a non-exiting, collecting onFailure callback (see
        // impl::InvokeOnFailure and RunFeature's own doc comment below).
        void AddBeforeAllHook(SuiteHookFunction hookFn) {
            m_beforeAllHooks.push_back(std::move(hookFn));
        }
        void AddAfterAllHook(SuiteHookFunction hookFn) {
            m_afterAllHooks.push_back(std::move(hookFn));
        }

        // Copies every step definition and hook from `other` into this
        // registry (appended after anything already registered). Copy
        // semantics: *this and other remain fully independent afterward -
        // mutating one after Merge() never affects the other. Lets a
        // consumer build a shared "library" StepRegistry once (e.g. via a
        // factory function returning one by value) and reuse it across many
        // scenarios/tests, optionally adding a few extra, test-specific step
        // definitions on top of the shared set (see examples/gherkin/
        // Gherkin{Bakery,Library}*.cpp). If both registries have a matching
        // pattern for the same keyword, TryMatch's first-match-wins linear
        // scan means whichever was registered/merged-in first still wins -
        // the same pre-existing behavior as registering a duplicate pattern
        // directly, nothing new introduced by Merge.
        void Merge(const StepRegistry& other) {
            for (std::size_t i = 0; i < impl::kStepKeywordCount; ++i) {
                auto& bucket = m_definitions.at(i);
                const auto& otherBucket = other.m_definitions.at(i);
                bucket.insert(bucket.end(), otherBucket.begin(), otherBucket.end());
            }
            m_beforeHooks.insert(m_beforeHooks.end(), other.m_beforeHooks.begin(), other.m_beforeHooks.end());
            m_afterHooks.insert(m_afterHooks.end(), other.m_afterHooks.begin(), other.m_afterHooks.end());
            m_beforeAllHooks.insert(
                m_beforeAllHooks.end(), other.m_beforeAllHooks.begin(), other.m_beforeAllHooks.end());
            m_afterAllHooks.insert(
                m_afterAllHooks.end(), other.m_afterAllHooks.begin(), other.m_afterAllHooks.end());
        }

        // --- Used internally by RunFeature()/RunScenario() below; not part
        // of the fluent registration API most consumers need directly. ---

        // Byte-identical forwarder for every caller that doesn't (yet) know
        // about Data Tables/Doc Strings: rawArgument == std::monostate
        // (impl::RawArgument{}, i.e. RawArgumentKind::None at invocation).
        [[nodiscard]] std::optional<StepFunction> TryMatch(impl::StepKeyword keyword, const std::string& text) const {
            return TryMatch(keyword, text, impl::RawArgument{});
        }

        // Real matching + captures + raw-argument threading.
        [[nodiscard]] std::optional<StepFunction> TryMatch(impl::StepKeyword keyword, const std::string& text,
                                                              const impl::RawArgument& rawArgument) const {
            for (const auto& definition : m_definitions.at(static_cast<std::size_t>(keyword))) {
                std::smatch match;
                if (std::regex_match(text, match, definition.pattern.regex)) {
                    std::vector<std::string> captures;
                    captures.reserve(!match.empty() ? match.size() - 1 : 0);
                    for (std::size_t i = 1; i < match.size(); ++i) {
                        captures.emplace_back(match[i].str());
                    }
                    const auto thunk = definition.thunk;
                    return StepFunction([thunk, captures = std::move(captures), rawArgument](TestContext& ctx) mutable -> bool {
                        return thunk(ctx, captures, rawArgument);
                    });
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] const std::vector<impl::Hook>& BeforeHooks() const {
            return m_beforeHooks;
        }
        [[nodiscard]] const std::vector<impl::Hook>& AfterHooks() const {
            return m_afterHooks;
        }
        [[nodiscard]] const std::vector<SuiteHookFunction>& BeforeAllHooks() const {
            return m_beforeAllHooks;
        }
        [[nodiscard]] const std::vector<SuiteHookFunction>& AfterAllHooks() const {
            return m_afterAllHooks;
        }

    private:
        // Dispatches one StepEntry<F> to the Register* method matching its
        // .keyword. Private helper for RegisterSteps() above. Sink
        // parameter taken BY VALUE (not StepEntry<F>&&): every call site
        // passes an rvalue (see RegisterSteps' fold expression), so the
        // by-value parameter is move-constructed for free, and taking it
        // by value - rather than by rvalue reference - lets this function
        // move its individual .pattern/.fn members out independently
        // (entry.keyword itself is read, not moved) without tripping
        // cppcoreguidelines-rvalue-reference-param-not-moved, which only
        // recognizes std::move(entry) as a whole, not member-wise moves.
        template<typename F>
        void RegisterOneStep(StepEntry<F> entry) {
            switch (entry.keyword) {
                case Keyword::Given: RegisterGiven(std::move(entry.pattern), std::move(entry.fn)); break;
                case Keyword::When:  RegisterWhen(std::move(entry.pattern), std::move(entry.fn)); break;
                case Keyword::Then:  RegisterThen(std::move(entry.pattern), std::move(entry.fn)); break;
                case Keyword::And:   RegisterAnd(std::move(entry.pattern), std::move(entry.fn)); break;
                case Keyword::But:   RegisterBut(std::move(entry.pattern), std::move(entry.fn)); break;
            }
        }

        template<typename F>
        void AddStepDefinition(impl::StepKeyword keyword, const std::string& pattern, F stepFn) {
            impl::CompiledStepPattern compiled = impl::CompileStepPattern(pattern);
            const std::size_t expectedArgs = impl::StepDefinitionArgCount<F>();
            // rawArgKind != None means expectedArgs == compiled.placeholderCount + 1
            // and F's trailing parameter is a valid raw-argument type (already
            // validated - or thrown as std::invalid_argument - inside
            // StepDefinitionRawArgKind above): a NEW, valid arity
            // relationship, distinct from (and never masking) the mismatch
            // check below, which stays byte-identical to pre-Feature-4
            // behavior for every other case (rawArgKind == None).
            const impl::RawArgumentKind rawArgKind = impl::StepDefinitionRawArgKind<F>(compiled.placeholderCount);
            if (rawArgKind == impl::RawArgumentKind::None && compiled.placeholderCount != expectedArgs) {
                throw std::invalid_argument(
                    "BabyBehave::Gherkin: step pattern '" + pattern + "' declares " +
                    std::to_string(compiled.placeholderCount) + " placeholder(s) but its step definition takes " +
                    std::to_string(expectedArgs) + " parameter(s) after TestContext&");
            }
            m_definitions.at(static_cast<std::size_t>(keyword))
                .push_back(impl::StepDefinition{ .pattern = std::move(compiled),
                                                  .patternText = pattern,
                                                  .thunk = impl::MakeStepThunk(std::move(stepFn), rawArgKind),
                                                  .rawArgKind = rawArgKind });
        }

        // One vector per impl::StepKeyword; And/But matched to own registered patterns.
        std::array<std::vector<impl::StepDefinition>, impl::kStepKeywordCount> m_definitions;
        std::vector<impl::Hook> m_beforeHooks;
        std::vector<impl::Hook> m_afterHooks;
        // Suite-level hooks (Feature 8): run once per RunFeature() call, in
        // registration order, never tag-filtered - see AddBeforeAllHook/
        // AddAfterAllHook above.
        std::vector<SuiteHookFunction> m_beforeAllHooks;
        std::vector<SuiteHookFunction> m_afterAllHooks;
    };

    // RunFeature() outcome: TestResults per Scenario, AND of all allPassed flags.
    // With the default onFailure callback (print + std::exit), in practice never
    // returns with allPassed==false (a failing Scenario/parse error exits first).
    // A consumer-supplied non-exiting onFailure callback is what lets RunFeature
    // actually return allPassed==false - see GherkinFailureCallback.
    struct FeatureResult {
        std::string featureName;
        std::vector<TestResult> scenarioResults;
        bool allPassed = true;

        // Convenience for a process main(): 0 if every scenario passed, 1 otherwise.
        [[nodiscard]] int ExitCode() const {
            return allPassed ? 0 : 1;
        }
    };

    namespace impl {

        // Build BabyBehaveTest step from Gherkin step: look up in registry,
        // or substitute synthetic failing step if no match. Always adds step.
        // deadline is additive/defaulted (nullptr): existing call sites that
        // don't pass one are unaffected and never even call WrapWithDeadlineCheck
        // - a Scenario with no @timeout tag takes the exact same no-wrapping
        // code path it always has.
        inline void AddParsedStepToTest(BabyBehaveTest& test, const ParsedStep& step, const StepRegistry& registry,
                                          std::string_view featureLabel, std::string_view namePrefix,
                                          const std::shared_ptr<ScenarioDeadline>& deadline = nullptr) {
            const std::string location = MakeFeatureLocation(featureLabel, step.line, step.column);
            std::string name = std::string(namePrefix) + step.text;
            std::optional<StepFunction> matched = registry.TryMatch(step.keyword, step.text, step.rawArgument);
            StepFunction stepFn = matched ? std::move(*matched)
                                       : StepFunction([text = step.text](TestContext&) -> bool {
                                             detail::PrintErrorLine(
                                                 "BabyBehave::Gherkin: no step definition matches: '" + text + "'");
                                             return false;
                                         });
            if (deadline) {
                stepFn = WrapWithDeadlineCheck(std::move(stepFn), deadline);
            }
            switch (step.keyword) {
                case StepKeyword::Given: test.AddStepAt<Precondition>(std::move(name), std::move(stepFn), location); break;
                case StepKeyword::When:  test.AddStepAt<Action>(std::move(name), std::move(stepFn), location); break;
                case StepKeyword::Then:  test.AddStepAt<Postcondition>(std::move(name), std::move(stepFn), location); break;
                case StepKeyword::And:   test.AddStepAt<And>(std::move(name), std::move(stepFn), location); break;
                case StepKeyword::But:   test.AddStepAt<But>(std::move(name), std::move(stepFn), location); break;
            }
        }

        // Default GherkinFailureCallback: print to stderr and hard-exit, matching
        // BabyBehave's library-wide default fail-hard philosophy. Byte-identical
        // to the pre-v0.8.1 behavior when a consumer doesn't pass their own callback.
        inline void DefaultGherkinFailureAction(std::string_view message) {
            detail::PrintErrorLine(std::string(message));
            std::exit(EXIT_FAILURE);
        }

        // Builds a single, concise ONE-LINE diagnostic for a failed
        // Scenario - "<featureLabel>:<scenarioLine>: scenario failed:
        // '<testName>' - K/N step(s) failed, first: [<stepLabel>]
        // <stepName>: <message>" plus an optional " (at <location>)" suffix
        // if the first failing step's location is non-empty. This is a
        // digest, not a dump: full per-step detail (every StepResult,
        // passed or failed, with its own message/location) remains
        // completely unaffected and available via
        // FeatureResult::scenarioResults[i].steps - this function only
        // changes what gets funneled through the single onFailure(...) call
        // for the Scenario (still invoked exactly once per failed Scenario,
        // never more/fewer times - see RunScenarioWithRetries below, the
        // only caller). Previously (pre-this-feature) this returned a
        // multi-line, '\n'-joined blob (one line per failed step) - that
        // was fine for a human reading raw stderr output but made
        // onFailure's argument awkward for structured log aggregation
        // (multi-line log lines, log processors splitting on '\n', etc.);
        // the one-line digest here is deliberately terse, pointing at the
        // FIRST failure only, on the theory that a consumer who needs every
        // failing step's full detail already has it via scenarioResults.
        inline std::string FormatScenarioFailureMessage(std::string_view featureLabel, const TestResult& result,
                                                          std::size_t scenarioLine) {
            std::size_t failedCount = 0;
            const StepResult* firstFailure = nullptr;
            for (const auto& step : result.steps) {
                if (!step.passed) {
                    ++failedCount;
                    if (firstFailure == nullptr) {
                        firstFailure = &step;
                    }
                }
            }
            std::string message = std::string(featureLabel) + ":" + std::to_string(scenarioLine) +
                                    ": scenario failed: '" + result.testName + "' - " +
                                    std::to_string(failedCount) + "/" + std::to_string(result.steps.size()) +
                                    " step(s) failed";
            if (firstFailure != nullptr) {
                message += ", first: [" + firstFailure->stepLabel + "] " + firstFailure->stepName + ": " +
                            firstFailure->message;
                if (!firstFailure->location.empty()) {
                    message += " (at " + firstFailure->location + ")";
                }
            }
            return message;
        }

        // Defense-in-depth serialization for onFailure invocations. Every
        // onFailure(...) call site in this namespace (the Feature-level parse
        // error path, the malformed-@timeout path, and the scenario-failure
        // path below) goes through this function instead of calling
        // onFailure directly. Single-threaded callers (enableParallelScenarios
        // == false, the default) pay for an uncontended lock/unlock - not
        // observable behaviorally - while parallel callers (RunFeature with
        // enableParallelScenarios == true) get a real guarantee: a consumer's
        // onFailure callback is never entered concurrently from two scenario
        // threads at once, even though nothing else about onFailure's
        // thread-safety is or can be guaranteed here (a callback that itself
        // touches shared state without its own synchronization can still
        // race with *other* code the consumer writes, just not with itself).
        //
        // This does NOT make the default onFailure (DefaultGherkinFailureAction,
        // which calls std::exit()) safe to use with enableParallelScenarios
        // == true: std::exit() begun on one thread while sibling scenario
        // threads are still mid-flight is a real hazard (their in-progress
        // BabyBehaveTest/TestContext objects can be torn down mid-destructor,
        // partially-flushed output can interleave, etc.) that a mutex around
        // the call itself cannot fix. See RunFeature's doc comment and the
        // dispatch code below for the full requirement: a consumer MUST pass
        // a non-exiting custom onFailure when enabling parallel execution.
        inline std::mutex& GherkinFailureCallbackMutex() {
            static std::mutex mutex;
            return mutex;
        }

        inline void InvokeOnFailure(const GherkinFailureCallback& onFailure, std::string_view message) {
            const std::scoped_lock<std::mutex> guard(GherkinFailureCallbackMutex());
            onFailure(message);
        }

        // Build and run BabyBehaveTest for a SINGLE attempt of scenario.
        // Execution order: Before hooks -> Background -> Scenario steps ->
        // After hooks. Does NOT consult ScenarioExecutionPolicy itself and
        // does NOT invoke onFailure - it is a pure "run once, report what
        // happened" primitive; @timeout/@retry policy resolution and
        // onFailure dispatch both live one level up, in
        // RunScenarioWithRetries, since a parse error is resolved once per
        // Scenario (not re-parsed per attempt) and onFailure must only ever
        // fire for the truly-final attempt (see RunScenarioWithRetries).
        //
        // @timeout handling: `timeout` (already resolved by the caller from
        // ScenarioExecutionPolicy::timeout) is turned into a brand-new
        // ScenarioDeadline HERE, fresh for this one attempt - this is what
        // guarantees a timeout on one retry attempt can never "use up"
        // budget that would otherwise apply to the next attempt's deadline.
        // If set, every Before hook/Background step/Scenario step (but
        // deliberately NOT After hooks - After hooks must always run to
        // completion regardless of timeout, mirroring the collect-failures-
        // forced-on contract above that already guarantees this for ordinary
        // failures) is wrapped with WrapWithDeadlineCheck via that one shared
        // ScenarioDeadline.
        inline TestResult RunScenarioAttempt(const ParsedFeature& feature, const ParsedScenario& scenario,
                                               const std::vector<std::string>& effectiveTags,
                                               const StepRegistry& registry, std::string_view featureLabel,
                                               const std::optional<std::chrono::milliseconds>& timeout) {
            const std::shared_ptr<ScenarioDeadline> deadline =
                timeout ? std::make_shared<ScenarioDeadline>(ScenarioDeadline{ .timeout = timeout }) : nullptr;

            // suppressGivenNarration=true: synthetic no-op setup (suppresses redundant line).
            // loc passed explicitly (not defaulted): the no-op lambda can never throw, so
            // m_contextSetupLocation is dead for this call regardless of its value - explicit
            // here only to avoid relying on the constructor's own default-argument evaluation
            // as this codebase's sole caller of it.
#if defined(__cpp_lib_source_location)
            BabyBehaveTest test(scenario.name, [](TestContext&) {}, true, std::source_location::current());
#else
            BabyBehaveTest test(scenario.name, [](TestContext&) {}, true);
#endif

            // Force collect-failures mode to guarantee After hooks run (reimplements
            // fail-hard below by inspecting TestResult::allPassed after Execute()).
            test.SetCollectFailuresMode(true);

            for (const auto& hook : registry.BeforeHooks()) {
                if (MatchesHookTags(hook, effectiveTags)) {
                    // WrapHookAsStep() called fresh per Scenario (Hook fn is copyable,
                    // but StepFunction is move-only).
                    StepFunction hookStep = WrapHookAsStep(hook.fn);
                    if (deadline) {
                        hookStep = WrapWithDeadlineCheck(std::move(hookStep), deadline);
                    }
                    test.AddStepAt<Precondition>("[Before] " + hook.label, std::move(hookStep),
                                                    MakeFeatureLocation(featureLabel, scenario.line, 0));
                }
            }
            for (const auto& step : feature.background) {
                AddParsedStepToTest(test, step, registry, featureLabel, "[Background] ", deadline);
            }
            for (const auto& step : scenario.steps) {
                AddParsedStepToTest(test, step, registry, featureLabel, "", deadline);
            }
            for (const auto& hook : registry.AfterHooks()) {
                if (MatchesHookTags(hook, effectiveTags)) {
                    // Not wrapped with a deadline check: After hooks must always
                    // run to completion regardless of @timeout.
                    test.AddStepAt<Postcondition>("[After] " + hook.label, WrapHookAsStep(hook.fn),
                                                     MakeFeatureLocation(featureLabel, scenario.line, 0));
                }
            }

            return test.Execute();
        }

        // Resolves @timeout/@retry policy once, then drives up to
        // policy.maxAttempts full attempts of scenario via RunScenarioAttempt,
        // stopping at the first successful attempt. This is the function
        // RunFeature actually calls (RunScenarioAttempt above is an internal
        // building block, not called directly from RunFeature).
        //
        // DESIGN DECISION - Before/After hooks and Background re-run on
        // EVERY attempt, not just once: each call to RunScenarioAttempt is a
        // complete, independent re-run of Before hooks -> Background ->
        // Scenario steps -> After hooks, from scratch, with a brand-new
        // BabyBehaveTest/TestContext (no state survives from one attempt to
        // the next). This is deliberate, not an oversight: it matches how a
        // human reading "@retry:3" would expect it to behave ("retry the
        // WHOLE scenario", not just its own steps while some earlier setup
        // silently carries over stale/partially-mutated state from the
        // failed attempt), and it matches the retry semantics of mainstream
        // real-world BDD/test tooling (e.g. Cucumber's cucumber-retry,
        // pytest-rerunfailures) that re-run fixtures/hooks on every retry
        // rather than trying to resume mid-scenario. A per-attempt-fresh
        // TestContext also sidesteps a whole class of subtle bugs a
        // hooks-run-once design would invite (e.g. a Before hook that opens
        // a resource into TestContext, a step that fails, then the SAME
        // resource being reused - possibly already partially consumed or
        // closed - across a retry).
        //
        // onFailure IS-ONLY-INVOKED-ON-FINAL-OUTCOME contract: onFailure
        // fires at most ONCE per Scenario, and only for either (a) never
        // (the first successful attempt, whichever attempt number that is),
        // or (b) the LAST attempt's failure, once policy.maxAttempts has
        // been exhausted with no success. Every intermediate (non-final)
        // failed attempt is deliberately silent - it is superseded by the
        // next attempt and never reported to scenarioResults/onFailure. This
        // funnels every real failure report through the exact same
        // InvokeOnFailure(...) mutex-guarded call site as before this
        // feature existed (see InvokeOnFailure's doc comment above) - retries
        // add a loop in front of it, they do not add a second path to it.
        //
        // Parallel-execution (enableParallelScenarios==true) safety: this
        // function runs entirely inside whichever Scenario's own std::async
        // task RunFeature dispatched it from (see RunFeature below) - the
        // retry loop is a private, sequential detail of that one task, with
        // no interaction with any other Scenario's task, so it "just works"
        // under parallel dispatch with zero changes needed here.
        inline TestResult RunScenarioWithRetries(const ParsedFeature& feature, const ParsedScenario& scenario,
                                                   const std::vector<std::string>& effectiveTags,
                                                   const StepRegistry& registry, std::string_view featureLabel,
                                                   const GherkinFailureCallback& onFailure) {
            const ScenarioExecutionPolicy policy = ParseScenarioExecutionPolicy(effectiveTags);
            if (!policy.parseError.empty()) {
                TestResult result;
                result.testName = scenario.name;
                result.allPassed = false;
                result.steps.push_back(StepResult{ .stepLabel = std::string(policy.parseErrorLabel),
                                                     .stepName = std::string(policy.parseErrorStepName),
                                                     .passed = false,
                                                     .message = policy.parseError,
                                                     .location = MakeFeatureLocation(featureLabel, scenario.line, 0) });
                InvokeOnFailure(onFailure, FormatScenarioFailureMessage(featureLabel, result, scenario.line));
                return result;
            }

            TestResult result;
            for (std::size_t attempt = 1; attempt <= policy.maxAttempts; ++attempt) {
                result = RunScenarioAttempt(feature, scenario, effectiveTags, registry, featureLabel, policy.timeout);
                if (result.allPassed) {
                    // First success - stop retrying. Nothing to report:
                    // onFailure is never invoked for a Scenario that
                    // ultimately passed, regardless of how many earlier
                    // attempts failed along the way.
                    return result;
                }
                if (attempt == policy.maxAttempts) {
                    // Every attempt has now been exhausted without success -
                    // THIS is the truly-final outcome, and the only failure
                    // ever reported for this Scenario (onFailure may
                    // exit/throw - the default does; if it returns normally,
                    // collect-failures mode was already forced on inside
                    // RunScenarioAttempt, so simply fall out of the loop and
                    // return result as-is, exactly like the pre-retry
                    // behavior did).
                    InvokeOnFailure(onFailure, FormatScenarioFailureMessage(featureLabel, result, scenario.line));
                }
                // else: an intermediate (non-final) failed attempt -
                // deliberately silent, no onFailure call. Loop around and
                // retry with a brand-new attempt (see the design-decision
                // comment above for why Before/After hooks and Background
                // are included in that fresh start).
            }
            return result;
        }

    } // namespace impl

    // Parse featureText (no file I/O) and run every Scenario against registry.
    // featureLabel used for diagnostics (pass filename for better messages).
    // Malformed feature or a failing Scenario invokes onFailure (default:
    // impl::DefaultGherkinFailureAction, printing to stderr and calling
    // std::exit(EXIT_FAILURE) - consistent with BabyBehaveTest's default
    // fail-hard behavior). A consumer-supplied onFailure that returns
    // normally instead of exiting/throwing lets RunFeature continue through
    // every remaining Scenario and return a FeatureResult with
    // allPassed==false, letting an advanced consumer redirect Gherkin-sourced
    // failures to their own test harness instead of the internal default.
    //
    // enableParallelScenarios (default false = today's exact behavior,
    // unchanged): when true, every Scenario is dispatched to its own
    // std::async(std::launch::async, ...) task instead of running serially
    // on the calling thread. This is safe to enable because
    // impl::RunScenarioAttempt (invoked, per attempt, by impl::RunScenarioWithRetries)
    // constructs a brand-new BabyBehaveTest/TestContext per Scenario (see its
    // definition above) - there is no mutable state shared between Scenarios,
    // the same "each scenario gets its own private TestContext" argument
    // examples/gherkin/GherkinLibraryConcurrentLending.cpp already relies on
    // for its own hand-rolled concurrent dispatch. StepRegistry is only ever
    // read from concurrently (TryMatch/BeforeHooks/AfterHooks are all const),
    // never mutated once RunFeature is running, so sharing one `registry`
    // across every task is safe too. Outline-expanded rows (see Examples:
    // support) are ordinary ParsedScenario entries by this point, so they
    // parallelize with zero special-casing; each task's own @timeout deadline
    // (if any) is likewise computed fresh inside impl::RunScenarioAttempt (a
    // NEW ScenarioDeadline per attempt, so a timeout on one @retry attempt
    // never "steals" budget from the next), so it "just works" per-task with
    // no changes needed here - the entire retry loop (@retry:N, if any) runs
    // privately inside that one Scenario's own std::async task, with no
    // cross-scenario interaction.
    //
    // *** SAFETY WARNING ***: the DEFAULT onFailure (impl::DefaultGherkinFailureAction,
    // which calls std::exit()) is NOT SAFE to use with enableParallelScenarios
    // == true. std::exit() invoked from one scenario's task while sibling
    // scenario tasks are still running on other threads is a genuine hazard -
    // their in-flight BabyBehaveTest/TestContext objects, and any static/global
    // destructors std::exit() triggers, race arbitrarily with those still-live
    // threads. A consumer enabling parallel execution MUST supply their own,
    // non-exiting onFailure callback (see impl::InvokeOnFailure above, and
    // GherkinLibraryConcurrentLending.cpp for a mutex-guarded example). This is
    // a documentation-only requirement: nothing here asserts or enforces it at
    // runtime.
    //
    // Behavioral divergence between serial and parallel mode when onFailure
    // itself throws or otherwise never returns: in SERIAL mode (the default),
    // a throwing onFailure genuinely halts the rest of the Feature - later
    // Scenarios in parsed.feature.scenarios are never even reached, because
    // the exception unwinds straight out of this function's for-loop. In
    // PARALLEL mode, every Scenario's std::async task is already dispatched
    // up front, before any of them may have failed - so if Scenario 2's
    // onFailure throws, Scenarios 3..N may already be running (or finished)
    // in the background regardless. Concretely: std::future::get() rethrows,
    // at the call site, any exception that escaped its async task (this is
    // standard-mandated behavior, not an implementation detail) - so the
    // `futures[i].get()` loop below propagates Scenario 2's exception out of
    // RunFeature the moment it reaches index 2, without ever calling .get()
    // on indices 3..N. However, this does NOT leak or abandon those later
    // tasks: as the exception unwinds through this function, the local
    // `futures` vector is destroyed, and a std::future obtained from
    // std::async(std::launch::async, ...) blocks in its destructor until its
    // task's shared state is ready - so by the time the exception has fully
    // left RunFeature, every dispatched Scenario has actually finished
    // running (verified empirically: a vector of such futures destructed
    // during unwinding does not return until the slowest task completes).
    // What IS lost is bookkeeping, not execution: Scenarios 3..N's TestResults
    // never get written into result.scenarioResults (that assignment is
    // skipped once the loop above throws), and if any of THOSE tasks also
    // threw from their own onFailure, that exception is silently discarded -
    // std::future's destructor observes a pending exception without
    // rethrowing it, unlike get(). In short: parallel mode's dispatch-then-
    // join structure means a throwing onFailure still lets every Scenario run
    // to completion, but only the first (by index) exception is ever
    // observed by the caller, and only that one Scenario's outcome is
    // reflected in the returned FeatureResult - the rest simply never had
    // their results collected.
    //
    // Suite-level Before-ALL/After-ALL hooks (Feature 8 - see
    // StepRegistry::AddBeforeAllHook/AddAfterAllHook): registry.BeforeAllHooks()
    // run once, serially, immediately below, before the Scenario-dispatch
    // loop begins (in either dispatch mode); registry.AfterAllHooks() run
    // once, serially, immediately after that loop's results have all been
    // collected. See the comments at each call site below for the exact
    // ordering/safety contract, and AddBeforeAllHook's own doc comment for
    // the After-ALL-never-runs-under-the-default-exiting-onFailure caveat.
    inline FeatureResult RunFeature(std::string_view featureText, StepRegistry& registry,
                                      std::string_view featureLabel = "<feature>",
                                      const GherkinFailureCallback& onFailure = impl::DefaultGherkinFailureAction,
                                      bool enableParallelScenarios = false) {
        const impl::ParseOutcome parsed = impl::ParseFeatureText(featureText);
        if (!parsed.ok) {
            // One onFailure(...) call PER accumulated structural error (see
            // impl::ParseOutcome/impl::ParseFeatureText) - featureLabel is
            // prepended here (the parser itself never sees it), giving a
            // final "<file>:<line>: parse error: <message>" one-liner per
            // error. Conservative invariant: a non-empty errors vector
            // means ZERO Scenarios ever execute below - this early return
            // guarantees that regardless of how many errors there are or
            // what impl::ParseFeatureText's best-effort parsed.feature
            // happens to contain.
            for (const std::string& error : parsed.errors) {
                impl::InvokeOnFailure(onFailure, std::string(featureLabel) + ":" + error);
            }
            return FeatureResult{ .featureName = std::string(), .scenarioResults = {}, .allPassed = false };
        }

        FeatureResult result;
        result.featureName = parsed.feature.name;
        // Preallocated by index (not push_back'd as each Scenario finishes):
        // this is what gives scenarioResults its declaration-order guarantee
        // in parallel mode below without needing any sort/comparator -
        // futures[i] already corresponds to parsed.feature.scenarios[i]
        // regardless of which task actually finishes first.
        result.scenarioResults.resize(parsed.feature.scenarios.size());

        // Suite-level Before-ALL hooks (Feature 8): run exactly once here,
        // serially/synchronously, before the Scenario-dispatch loop below
        // begins - identically for both dispatch modes (enableParallelScenarios
        // true or false). Because every Before-ALL hook has already returned
        // by the time any Scenario (parallel or not) starts running, any
        // suite-wide state a Before-ALL hook establishes is fully visible to
        // every Scenario without introducing a new data race: parallel
        // Scenarios never run concurrently with Before-ALL hooks themselves,
        // only ever after them.
        for (const SuiteHookFunction& beforeAllHook : registry.BeforeAllHooks()) {
            beforeAllHook();
        }

        if (!enableParallelScenarios) {
            for (std::size_t i = 0; i < parsed.feature.scenarios.size(); ++i) {
                const auto& scenario = parsed.feature.scenarios[i];
                const std::vector<std::string> effectiveTags = impl::UnionTags(parsed.feature.tags, scenario.tags);
                result.scenarioResults[i] = impl::RunScenarioWithRetries(parsed.feature, scenario, effectiveTags,
                                                                            registry, featureLabel, onFailure);
            }
        } else {
            std::vector<std::future<TestResult>> futures;
            futures.reserve(parsed.feature.scenarios.size());
            for (const auto& scenario : parsed.feature.scenarios) {
                // effectiveTags is a per-iteration local about to go out of
                // scope - it MUST be captured by value below. parsed/registry/
                // featureLabel/onFailure are safe to capture by reference only
                // because every future here is .get()-joined (or, on the
                // exception path documented above, blocked-on-during-
                // destruction) before RunFeature returns - none of their
                // referents can dangle while any task might still touch them.
                const std::vector<std::string> effectiveTags = impl::UnionTags(parsed.feature.tags, scenario.tags);
                futures.push_back(std::async(std::launch::async,
                    [&parsed, &scenario, effectiveTags, &registry, featureLabel, &onFailure]() {
                        return impl::RunScenarioWithRetries(parsed.feature, scenario, effectiveTags, registry,
                                                               featureLabel, onFailure);
                    }));
            }
            for (std::size_t i = 0; i < futures.size(); ++i) {
                result.scenarioResults[i] = futures[i].get();
            }
        }

        // Suite-level After-ALL hooks (Feature 8): both dispatch branches
        // above have already fully joined by this point (the serial for-loop
        // has returned every result; the parallel branch's `futures[i].get()`
        // loop has already blocked until every task finished) - so, exactly
        // like Before-ALL above, After-ALL hooks here run serially, once,
        // with zero in-flight Scenario execution to race against.
        //
        // *** SAFETY CAVEAT ***: this line is only ever reached if every
        // Scenario failure so far went through onFailure without exiting/
        // throwing. With the DEFAULT onFailure (impl::DefaultGherkinFailureAction,
        // which calls std::exit()), a failing Scenario anywhere above means
        // the process has already exited and this line - along with every
        // registered After-ALL hook - never runs. This mirrors the exact
        // same fail-hard-by-default caveat documented for
        // enableParallelScenarios above: After-ALL hooks used for suite-wide
        // cleanup that MUST run even on failure require a non-exiting,
        // collecting onFailure callback.
        for (const SuiteHookFunction& afterAllHook : registry.AfterAllHooks()) {
            afterAllHook();
        }

        // Both branches converge here: same post-loop AND-reduction regardless
        // of dispatch strategy.
        for (const TestResult& scenarioResult : result.scenarioResults) {
            result.allPassed = result.allPassed && scenarioResult.allPassed;
        }
        return result;
    }

    // Reads an entire .feature file from disk into a std::string. Plain
    // std::ifstream, ordinary std::filesystem::path semantics (absolute
    // used as-is, relative resolved against the process's current working
    // directory) - no build-system/CMake-define dependency of any kind
    // (contrast with examples/gherkin/LoadFeatureFile.hpp's example-only
    // helper, which depends on a per-target BABYBEHAVE_GHERKIN_FEATURES_DIR
    // compile definition; this one does not and must not). Throws
    // std::runtime_error (with the path in the message) if the file can't
    // be opened. Does NOT violate "RunFeature reads text only, no file
    // I/O of its own" - RunFeature() itself is untouched; this is a
    // separate, optional free function a caller may use to produce the
    // std::string RunFeature()/FeatureFromFile() ultimately consumes.
    [[nodiscard]] inline std::string LoadFeatureFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("BabyBehave::Gherkin::LoadFeatureFile: could not open file: " + path.string());
        }
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return contents;
    }

    // Fluent builder wrapping a single RunFeature() call. Unlike RunFeature()
    // itself (std::string_view, zero-copy), FeatureRun OWNS a std::string
    // copy of the feature text - deliberately, so a temporary (e.g.
    // LoadFeatureFile()'s return value) can be handed to Feature()/
    // FeatureFromFile() safely with no dangling risk. This one extra copy
    // is paid once per Feature()/FeatureFromFile() call, not once per
    // Scenario/step.
    class FeatureRun {
    public:
        FeatureRun(std::string featureText, StepRegistry& registry)
            : m_featureText(std::move(featureText)), m_registry(registry) {}

        // Sets the diagnostic label passed through to RunFeature() (e.g. a filename).
        FeatureRun& Label(std::string_view label) {
            m_label = std::string(label);
            return *this;
        }

        // Sets the onFailure callback passed through to RunFeature().
        FeatureRun& OnFailure(GherkinFailureCallback onFailure) {
            m_onFailure = std::move(onFailure);
            return *this;
        }

        // Enables/disables RunFeature()'s enableParallelScenarios flag.
        //
        // *** SAFETY WARNING (same caveats as RunFeature()'s
        // enableParallelScenarios parameter - repeated here in full since
        // this is a separate entry point a consumer may discover
        // independently of docs/design/gherkin-support.md) ***:
        //
        // 1. The DEFAULT onFailure (impl::DefaultGherkinFailureAction, which
        //    calls std::exit()) is NOT thread-safe and must not be used with
        //    Parallel(true) - calling std::exit() concurrently from multiple
        //    scenario threads is a data race and undefined behavior. Supply
        //    a non-exiting custom OnFailure() (e.g. a mutex-guarded vector
        //    collecting messages) whenever Parallel(true) is used.
        // 2. Serial vs. parallel exception-handling diverges: in serial mode
        //    a throwing onFailure halts the rest of the Feature immediately;
        //    in parallel mode every Scenario's std::async task is already
        //    dispatched up front, so later Scenarios may already be running
        //    (or finished) by the time an earlier one's onFailure throws -
        //    only the first (by index) exception is ever observed by the
        //    caller, and the rest of that task's bookkeeping is lost (though
        //    every dispatched task still runs to completion, since a
        //    std::future's destructor blocks until its task finishes).
        // 3. Per-scenario hook closures (AddBeforeHook/AddAfterHook) run
        //    concurrently across scenario threads under Parallel(true) - any
        //    mutable state a hook closure captures by reference/pointer is
        //    the consumer's own synchronization responsibility; the
        //    framework does not serialize or lock access to it.
        // 4. There is no internal thread-pool cap: every Scenario (including
        //    every expanded row of a Scenario Outline) gets its own
        //    std::launch::async task/OS thread. A large Examples: table
        //    combined with Parallel(true) can spawn hundreds of OS threads.
        //
        // Consumers enabling Parallel(true) must also link Threads::Threads
        // in their own CMake target (find_package(Threads REQUIRED)) -
        // std::async requires platform threading support.
        FeatureRun& Parallel(bool enable = true) {
            m_parallel = enable;
            return *this;
        }

        // Runs the Feature via RunFeature() - no duplicated logic, this is a
        // pure forwarder using whatever was configured via Label()/OnFailure()/Parallel().
        [[nodiscard]] FeatureResult Run() const {
            return RunFeature(m_featureText, m_registry, m_label, m_onFailure, m_parallel);
        }

    private:
        std::string m_featureText;
        StepRegistry& m_registry;
        std::string m_label = "<feature>";
        GherkinFailureCallback m_onFailure = impl::DefaultGherkinFailureAction;
        bool m_parallel = false;
    };

    // Factory for FeatureRun from an in-memory feature text (e.g. a raw
    // string literal). See FeatureRun's doc comment for why this copies
    // featureText rather than taking a std::string_view like RunFeature().
    [[nodiscard]] inline FeatureRun Feature(std::string featureText, StepRegistry& registry) {
        return {std::move(featureText), registry};
    }

    // Factory for FeatureRun that loads its feature text from disk via
    // LoadFeatureFile() above, and defaults its Label() to the given path.
    [[nodiscard]] inline FeatureRun FeatureFromFile(const std::filesystem::path& path, StepRegistry& registry) {
        return FeatureRun(LoadFeatureFile(path), registry).Label(path.string());
    }

    // Promotes a mutex+vector+lambda pattern that several examples
    // (GherkinCustomFailureHandler.cpp, GherkinCollectFailures.cpp,
    // GherkinBakeryConcurrentOrderProcessing.cpp,
    // GherkinLibraryConcurrentLending.cpp) already hand-roll identically:
    // a GherkinFailureCallback that collects every message into a
    // consumer-owned std::vector<std::string> instead of the default
    // impl::DefaultGherkinFailureAction's print-and-std::exit() behavior.
    // Usable directly as RunFeature()'s onFailure argument or
    // FeatureRun::OnFailure(...) - GherkinFailureCallback is
    // std::function<void(std::string_view)>, and a CollectingFailureHandler
    // instance converts to that cleanly (operator() is const and takes
    // std::string_view, matching the callback signature exactly).
    //
    // Deliberately non-exiting - never calls std::exit()/std::abort(),
    // never throws - which is the entire point: it lets RunFeature() keep
    // going through every remaining Scenario (and, across multiple
    // RunFeature() calls, every remaining Feature) instead of the default
    // callback's hard stop on the first failure.
    //
    // Thread-safe by construction (every operator() call takes m_mutex
    // before touching m_sink), so it is also safe to use as the onFailure
    // callback under enableParallelScenarios==true/FeatureRun::Parallel() -
    // see docs/design/gherkin-support.md's "Parallel scenario execution"
    // section for why the DEFAULT callback is unsafe there (concurrent
    // std::exit() calls) and why a non-exiting, mutex-guarded callback like
    // this one is required instead.
    //
    // m_sink is stored BY REFERENCE, not owned: the caller keeps their own
    // std::vector<std::string> alive for at least as long as this handler
    // (and whatever RunFeature()/FeatureRun call it's passed to) is in use -
    // identical lifetime contract to the hand-written lambdas this class
    // replaces, which all captured their vector by reference too.
    //
    // m_mutex is a shared_ptr<mutex>, NOT a plain std::mutex member,
    // deliberately: GherkinFailureCallback is a std::function, whose
    // constructor from an arbitrary callable requires the target type to
    // be CopyConstructible (see std::function's Callable requirements) - a
    // plain std::mutex member would make CollectingFailureHandler itself
    // non-copy-constructible (std::mutex's copy constructor is deleted),
    // breaking that requirement outright. A shared_ptr<mutex> keeps
    // CollectingFailureHandler copyable while guaranteeing every copy
    // (however many std::function/the caller ends up making) still
    // synchronizes through the exact same underlying mutex instance -
    // giving each COPY its own fresh mutex instead would silently defeat
    // thread-safety (two different mutexes "protecting" one shared m_sink
    // is equivalent to no synchronization at all).
    class CollectingFailureHandler {
    public:
        explicit CollectingFailureHandler(std::vector<std::string>& sink)
            : m_sink(sink), m_mutex(std::make_shared<std::mutex>()) {}

        void operator()(std::string_view message) const {
            const std::scoped_lock<std::mutex> lock(*m_mutex);
            m_sink.emplace_back(message);
        }

    private:
        std::vector<std::string>& m_sink;
        std::shared_ptr<std::mutex> m_mutex;
    };

} // namespace Gherkin
#endif // BABYBEHAVE_DISABLE_GHERKIN

} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP