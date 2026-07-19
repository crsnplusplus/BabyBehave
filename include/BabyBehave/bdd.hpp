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
#include <concepts>
#include <optional>
#include <regex>
#include <tuple>
#endif


namespace BabyBehave::BDD {

    namespace detail {
        // Cheaper than std::endl (no forced flush).
        inline constexpr char kNewLine = '\n';

        // Runtime toggle for narration via BABYBEHAVE_QUIET env var.
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

        // Print diagnostic to std::cerr, respecting NarrationEnabledFlag.
        inline void PrintErrorLine(const std::string& text) {
            if (!NarrationEnabledFlag()) {
                return;
            }
            std::cerr << text << kNewLine;
        }

        // Plain (live), Arrow/Tree (buffered, rendered at Execute's end).
        enum class NarrationStyle : std::uint8_t {
            Plain,
            Arrow,
            Tree,
        };

        // Maps BABYBEHAVE_STYLE to NarrationStyle ("plain"/"arrow"/"tree",
        // case-sensitive; defaults to Plain). Pulled out as a pure function
        // for coverage: each of four cases becomes one direct test call.
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

        // Cached from BABYBEHAVE_STYLE, overrideable via SetNarrationStyle().
        inline NarrationStyle& NarrationStyleFlag() {
            static NarrationStyle style = ParseNarrationStyleEnv(std::getenv("BABYBEHAVE_STYLE"));
            return style;
        }

        // Step types for Arrow/Tree renderers. Given is stored in m_testName.
        enum class StepKindTag : std::uint8_t {
            With,
            When,
            Then,
            And,
            Or,
            But,
        };

        // Detail steps nest one level under primary steps (With under GIVEN;
        // And/Or/But under THEN); Tree nests, Arrow marks with "+".
        constexpr bool IsNarrationDetailKind(StepKindTag kind) {
            return kind == StepKindTag::With || kind == StepKindTag::And ||
                   kind == StepKindTag::Or || kind == StepKindTag::But;
        }

        // std::unreachable() after exhaustive switch: falling off is genuinely
        // impossible, not just unhandled; a defensive return would be uncoverable.
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

        // One step buffered by NarrateStep() for Arrow/Tree rendering.
        struct NarrationStepEntry {
            StepKindTag kind;
            std::string name;
        };

        // lines is never empty; builds result by concatenating with newlines.
        inline std::string JoinNarrationLines(const std::vector<std::string>& lines) {
            std::string result = lines.front();
            for (auto it = std::next(lines.begin()); it != lines.end(); ++it) {
                result += kNewLine;
                result += *it;
            }
            return result;
        }

        // "-> Given a: X" / "    + With: Y" / etc. (Detail steps get "+", primary get "->").
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

        // Left-justifies to fixed width (7 chars) so all step names align.
        inline std::string PadTreeLabel(std::string_view label) {
            constexpr std::size_t kFieldWidth = 7;
            return label.size() < kFieldWidth ? std::string(label) + std::string(kFieldWidth - label.size(), ' ')
                                               : std::string(label);
        }

        // Unicode glyphs as \u escapes for portable encoding (vs literal UTF-8).
        inline constexpr std::string_view kTreeCheckMark = "\u2713";         // CHECK MARK
        inline constexpr std::string_view kTreeCrossMark = "\u2717";        // BALLOT X
        inline constexpr std::string_view kTreeVerticalBar = "\u2502";      // BOX DRAWINGS LIGHT VERTICAL
        inline constexpr std::string_view kTreeBranchMid = "\u251c\u2500 "; // VERTICAL-AND-RIGHT, HORIZONTAL, space
        inline constexpr std::string_view kTreeBranchLast = "\u2570\u2500 "; // ARC-UP-AND-RIGHT, HORIZONTAL, space

        // Tree continuation prefix: "   " if isLast, else vertical bar + two spaces.
        inline std::string TreeContinuation(bool isLast) {
            return isLast ? "   " : std::string(kTreeVerticalBar) + "  ";
        }

        // Top-level branch (WHEN or THEN); THEN may have And/Or/But children.
        struct TreeBranch {
            std::string_view label;
            const std::string* name;
            const std::vector<const NarrationStepEntry*>* children;
        };

        // Appends indented box-drawing lines for each child (WITH or And/Or/But).
        // Last child gets arc-corner glyph; others get tee glyph.
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

        // Unicode tree: GIVEN/WHEN/THEN top-level siblings; WITH nests under
        // GIVEN; And/Or/But nest under last THEN (or promoted to top-level).
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
        // Formats source_location as "file:line" for diagnostics.
        inline std::string FormatLocation(const std::source_location& loc) {
            std::string result(loc.file_name());
            result += ':';
            result += std::to_string(loc.line());
            return result;
        }

        // Consolidates source_location capture across call sites (replaces
        // duplicated #if blocks in constructor/AddStep/GivenAImpl).
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

        // Step-type labels (used as both StepResult::stepLabel and exception callback labels).
        inline constexpr std::string_view kPreconditionLabel = "Precondition";
        inline constexpr std::string_view kActionLabel = "Action";
        inline constexpr std::string_view kPostconditionLabel = "Postcondition";
        inline constexpr std::string_view kAndLabel = "And";
        inline constexpr std::string_view kOrLabel = "Or";
        inline constexpr std::string_view kButLabel = "But";
        inline constexpr std::string_view kContextSetupLabel = "ContextSetup";

        // Message for catch-all (...) exception handler.
        inline constexpr std::string_view kUnknownExceptionMessage = "unknown non-std::exception type thrown";

        // Context-setup exception prefix (used in both exception handlers).
        inline constexpr std::string_view kContextSetupExceptionPrefix = "Exception caught in Context Setup: ";

        // Remaining user-facing string literals (gathered here for consistency).
        inline constexpr std::string_view kKeyNotFoundPrefix = "Key not found: ";
        inline constexpr std::string_view kGivenPrefix = "Given a: ";
        inline constexpr std::string_view kWithPrefix = "    With: ";
        inline constexpr std::string_view kWhenPrefix = "    When: ";
        inline constexpr std::string_view kThenPrefix = "    Then: ";
        inline constexpr std::string_view kAndPrefix = "    And: ";
        inline constexpr std::string_view kOrPrefix = "    Or: ";
        inline constexpr std::string_view kButPrefix = "    But: ";
        // Default onException callback prefix (distinct from context-setup failures).
        inline constexpr std::string_view kDefaultExceptionCallbackPrefix = "Exception caught in ";
        inline constexpr std::string_view kOnConditionNotVerifiedCallbackThrewMessage =
            "BabyBehave: onConditionNotVerified callback itself threw an exception; ignoring to avoid std::terminate()";
        inline constexpr std::string_view kOnExceptionCallbackThrewMessage =
            "BabyBehave: onException callback itself threw an exception; ignoring to avoid std::terminate()";

        // Compile-time failure messages/exception descriptions. Passing steps
        // pay zero allocation cost (only materialized on failure path).
        // Groups follow executeStep() order; And/Or/But have *ConditionLabel.

        // Precondition
        inline constexpr std::string_view kPreconditionFailedMessage = "Precondition failed";

        // Action
        inline constexpr std::string_view kActionFailedMessage = "Action failed";

        // Postcondition
        inline constexpr std::string_view kPostconditionFailedMessage = "Postcondition failed";

        // And
        inline constexpr std::string_view kAndConditionLabel = "And condition";
        inline constexpr std::string_view kAndConditionFailedMessage = "And condition failed";

        // Or
        inline constexpr std::string_view kOrConditionLabel = "Or condition";
        inline constexpr std::string_view kOrConditionFailedMessage = "Or condition failed";

        // But
        inline constexpr std::string_view kButConditionLabel = "But condition";
        inline constexpr std::string_view kButConditionFailedMessage = "But condition failed";

        // Appends " (at file:line)" suffix when location is non-empty.
        inline std::string AppendLocationSuffix(const std::string& text, const std::string& location) {
            return location.empty() ? text : (text + " (at " + location + ")");
        }

        // C++20 transparent hash for heterogeneous lookup. Lets Get() avoid
        // materializing owning std::string for find(std::string_view).
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

    // NOTE: TestContext is NOT thread-safe (it is backed by a plain std::unordered_map
    // with no internal synchronization). Consumers running scenarios in parallel (e.g.
    // via std::async or parallel ctest) must not share a single TestContext instance
    // across threads.
    class TestContext {
    private:
        std::unordered_map<std::string, std::any, detail::TransparentStringHash, std::equal_to<>> m_objects;

        // SoftCheck results side-channel. Written during Check() calls,
        // cleared by BabyBehaveTest before each step. Not thread-local.
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
        // key: const std::string& (no heterogeneous operator[] in unordered_map<C++26).
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = std::move(obj);
        }

        // key: std::string_view for zero-alloc lookup via transparent hash.
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

        // Opt-in, compile-time-checked type-safe key (delegates to string storage).
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
            // key.name is a const char*; Get<T>(std::string_view) below
            // constructs the view for free (no allocation), unlike the
            // std::string(key.name) this used to require.
            return Get<T>(key.name);
        }
    };

    // Opt-in recorder for multiple named sub-checks per step (see example below).
    // Purely additive: unused steps behave as before. Example:
    //   bool MyStep(TestContext& ctx) {
    //       SoftCheck checks(ctx);
    //       checks.Check("has id", someId > 0);
    //       checks.Check("name matches", name == "expected");
    //       return checks.AllPassed();
    //   }
    class SoftCheck {
    public:
        explicit SoftCheck(TestContext& context) : m_context(context) {}

        // Record sub-check, return condition (supports early-return patterns).
        bool Check(std::string label, bool condition, std::string message = "") {
            m_context.RecordSoftCheck(SoftCheckResult{ .label = std::move(label), .passed = condition, .message = std::move(message) });
            if (!condition) {
                m_allPassed = false;
            }
            return condition;
        }

        // AND of all Check() calls (defaults to true if none called).
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

    // Callback types (plain std::function; not move-only unlike StepFunction).
    using ConditionNotVerifiedCallback = std::function<void(const std::string& errorMsg)>;
    using ExceptionCallback = std::function<void(const std::string& step, const std::exception&)>;

    struct Precondition { StepFunction fn; };
    struct Action { StepFunction fn; };
    struct Postcondition { StepFunction fn; };
    struct And { StepFunction fn; };
    struct Or { StepFunction fn; };
    struct But { StepFunction fn; };

    // StepMeta table removes duplicated try/catch/Verify logic by indexing
    // constants (k*Label, k*FailedMessage) per step type. One template
    // instantiation per StepType, same dispatch as before.
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
    // message is empty when passed; location is "file:line" if available.
    struct StepResult {
        std::string stepLabel;
        std::string stepName;
        bool passed = true;
        std::string message;
        std::string location;
    };

    // Scenario outcome: allPassed is AND of all StepResult::passed.
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
        // loc defaults to caller's location. suppressGivenNarration is false
        // by default (used by Gherkin for synthetic no-op setup functions).
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

        // Only std::bad_alloc could escape; all step/callback exceptions caught internally.
        // NOLINTNEXTLINE(bugprone-exception-escape)
        ~BabyBehaveTest() {
            // Execute() is idempotent; no-op if already called.
            Execute();
        }

        BabyBehaveTest(const BabyBehaveTest&) = delete;
        BabyBehaveTest& operator=(const BabyBehaveTest&) = delete;

        // Move deleted: defaulted move would double-execute (m_executed copied
        // to both moved-to and moved-from). GivenAImpl returns by value
        // (copy-elision in C++17 avoids calling this anyway).
        BabyBehaveTest(BabyBehaveTest&&) = delete;
        BabyBehaveTest& operator=(BabyBehaveTest&&) = delete;

        void SetOnConditionNotVerifiedCallback(ConditionNotVerifiedCallback callback) {
            m_onConditionNotVerifiedCallback = std::move(callback);
        }

        void SetOnExceptionCallback(ExceptionCallback callback) {
            m_onExceptionCallback = std::move(callback);
        }

        // Opt-in: record failures to TestResult instead of exit. Fluent.
        BabyBehaveTest& SetCollectFailuresMode(bool enabled = true) {
            m_collectFailures = enabled;
            return *this;
        }

#if defined(__cpp_lib_source_location)
        // loc defaults to caller's location (captured by macro/direct call).
        // name is a sink parameter (moved, saves string copy per registration).
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

        // Bypass automatic source_location capture: takes explicit location string.
        // Used by Gherkin to attribute steps to .feature file/line/column, not interpreter.
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

        // Run scenario (setup + steps), return cached TestResult. Idempotent.
        // Normally invoked by destructor; call manually with SetCollectFailuresMode
        // to inspect results before destruction. Example:
        //   BabyBehaveTest test = GivenA(SetupContext);
        //   test.SetCollectFailuresMode(true).With(...).When(...).Then(...);
        //   const TestResult& result = test.Execute();  // inspect results
        const TestResult& Execute() {
            if (m_executed) {
                return m_result;
            }
            m_executed = true;

            // Plain prints Given immediately; Arrow/Tree hold m_testName for end rendering.
            // Suppressed when m_suppressGivenNarration (Gherkin's synthetic setup).
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

            // Plain prints blank line; Arrow/Tree render buffered block now.
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

        // Print step immediately (Plain) or buffer for end rendering (Arrow/Tree).
        void NarrateStep(detail::StepKindTag kind, std::string_view plainPrefix, const std::string& name) {
            if (detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
                detail::PrintLine(std::string(plainPrefix) + name);
            } else {
                m_narrationSteps.push_back(detail::NarrationStepEntry{ .kind = kind, .name = name });
            }
        }

        // One function per StepType (via template), indexed by StepMeta<T> instead of
        // duplicating try/catch/Verify logic six times. Single std::visit dispatch site.
        // NOTE: T& (not const T&) is deliberate: GCC/libstdc++'s std::move_only_function
        // ::operator() is non-const, so step must be non-const to invoke step.fn(context).
        template<typename T>
        void executeStep(const std::string& name, T& step, const std::string& location) {
            // Clear soft-check results; guarantees fresh list for step.fn() call.
            m_context.ClearSoftCheckResults();
            using Meta = detail::StepMeta<T>;
            // All arguments are compile-time std::string_view (from StepMeta).
            // Passing steps allocate nothing for labels/messages.
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

        // Append ": label (msg); label2 (msg2)" for each failed SoftCheck to out.
        // Only failed checks included. Appends to out to avoid extra allocations.
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

        // Handle step condition (on failure: invoke callback or record to m_result).
        // In collect-failures mode: record all steps; callback is skipped.
        // errorMsg/stepLabel are std::string_view; passing steps allocate nothing.
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

        // Invoke exception callback (same collect-failures behavior as VerifyCondition).
        // Defers "step" label conversion to std::string until callback invocation.
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

        // True for Gherkin synthetic no-op setup; suppresses redundant "Given a: <name>".
        bool m_suppressGivenNarration = false;

        TestContext m_context;
        std::vector<Step> m_steps;
        std::vector<std::string> m_stepLocations;

        // Buffered steps for Arrow/Tree rendering. Empty under Plain style.
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
    // steps, @tags, # comments, {int}/{float}/{string}/{word}, Before/After hooks
    // (tag-filtered, AND/subset match). NOT covered (parse error): Rule, Scenario
    // Outline/Examples, Doc Strings, Data Tables, i18n keywords.
#if !defined(BABYBEHAVE_DISABLE_GHERKIN)
namespace Gherkin {

    // void(TestContext&), wrapped to StepFunction via impl::WrapHookAsStep.
    using HookFunction = std::function<void(TestContext&)>;

    // Invoked once per Gherkin-sourced failure (a parse error, or a failing
    // Scenario) - never once-per-RunFeature-call, so plain std::function (not
    // move_only_function) is required: it may be invoked multiple times
    // across a single RunFeature() call (once per failing Scenario). Default
    // is impl::DefaultGherkinFailureAction (print + std::exit), preserving
    // today's fail-hard behavior for consumers who don't pass one.
    using GherkinFailureCallback = std::function<void(std::string_view)>;

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

        template<typename T>
        T ConvertCapture(const std::string& raw) {
            if constexpr (std::is_same_v<T, int>) {
                return std::stoi(raw);
            } else if constexpr (std::is_same_v<T, long>) {
                return std::stol(raw);
            } else if constexpr (std::is_same_v<T, long long>) {
                return std::stoll(raw);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(raw);
            } else if constexpr (std::is_same_v<T, float>) {
                return std::stof(raw);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return raw;
            } else {
                static_assert(kAlwaysFalseGherkinType<T>,
                    "Unsupported step-definition parameter type; use int/long/long long/float/double/std::string");
                return T{};
            }
        }

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

        template<typename ArgsTuple, typename F, std::size_t... I>
        bool InvokeWithCaptures(F& func, TestContext& ctx, const std::vector<std::string>& captures,
                                  std::index_sequence<I...> seq) {
            std::ignore = seq;
            return static_cast<bool>(func(ctx, ConvertCapture<std::tuple_element_t<I + 1, ArgsTuple>>(captures[I])...));
        }

        // Type-erase step definition to bool(TestContext&, captures).
        // Converts captures to F's parameter types before invoking.
        template<typename F>
        std::function<bool(TestContext&, const std::vector<std::string>&)> MakeStepThunk(F stepFn) {
            using ArgsTuple = CallableSignature<std::decay_t<F>>::ArgsTuple;
            constexpr std::size_t placeholderCount = StepDefinitionArgCount<F>();
            return [stepFn = std::move(stepFn)](TestContext& ctx, const std::vector<std::string>& captures) mutable -> bool {
                if (captures.size() != placeholderCount) {
                    detail::PrintErrorLine(
                        "BabyBehave::Gherkin: internal error - captured argument count does not match step definition");
                    return false;
                }
                return InvokeWithCaptures<ArgsTuple>(stepFn, ctx, captures, std::make_index_sequence<placeholderCount>{});
            };
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
        struct Hook {
            std::vector<std::string> tags;
            HookFunction fn;
            std::string label;
        };

        struct StepDefinition {
            CompiledStepPattern pattern;
            std::string patternText;
            std::function<bool(TestContext&, const std::vector<std::string>&)> thunk;
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

        // --- Parser ----------------------------------------------------------
        // std::string_view in, no file I/O (header-only design).

        struct ParsedStep {
            StepKeyword keyword = StepKeyword::Given;
            std::string text;
            std::size_t line = 0;
            std::size_t column = 0;
        };

        struct ParsedScenario {
            std::string name;
            std::vector<std::string> tags;
            std::vector<ParsedStep> steps;
            std::size_t line = 0;
        };

        struct ParsedFeature {
            std::string name;
            std::vector<std::string> tags;
            std::vector<ParsedStep> background;
            std::vector<ParsedScenario> scenarios;
        };

        // Plain ok/error/value result (not std::expected; no chaining needed).
        struct ParseOutcome {
            bool ok = false;
            std::string error;
            ParsedFeature feature;
        };

        inline ParseOutcome MakeParseError(std::size_t lineNo, std::string_view message) {
            ParseOutcome outcome;
            outcome.ok = false;
            outcome.error = "BabyBehave::Gherkin: line " + std::to_string(lineNo) + ": " + std::string(message);
            return outcome;
        }

        // Reject out-of-scope constructs (Doc Strings, Data Tables, Rule, etc).
        inline std::optional<ParseOutcome> RejectIfUnsupportedConstruct(std::string_view trimmed, std::size_t lineNo) {
            if (trimmed.starts_with(R"(""")")) {
                return MakeParseError(lineNo, "Doc strings are not supported in this version "
                                               "(see docs/design/gherkin-support.md)");
            }
            if (trimmed.front() == '|') {
                return MakeParseError(lineNo, "Data tables are not supported in this version "
                                               "(see docs/design/gherkin-support.md)");
            }
            if (trimmed.starts_with("Rule:")) {
                return MakeParseError(lineNo, "'Rule:' is not supported in this version "
                                               "(see docs/design/gherkin-support.md)");
            }
            if (trimmed.starts_with("Scenario Outline:") || trimmed.starts_with("Scenario Template:") ||
                trimmed.starts_with("Examples:") || trimmed.starts_with("Scenarios:")) {
                return MakeParseError(lineNo, "Scenario Outline/Examples are not supported in this version "
                                               "(see docs/design/gherkin-support.md)");
            }
            return std::nullopt;
        }

        // Handle Feature: line; error if multiple Features.
        inline std::optional<ParseOutcome> HandleFeatureLine(ParsedFeature& feature, bool& haveFeature,
                                                               std::vector<std::string>& pendingTags,
                                                               std::string_view trimmed, std::size_t lineNo) {
            if (haveFeature) {
                return MakeParseError(lineNo, "multiple 'Feature:' sections are not supported");
            }
            haveFeature = true;
            feature.name = std::string(TrimView(trimmed.substr(std::string_view("Feature:").size())));
            feature.tags = std::move(pendingTags);
            pendingTags.clear();
            return std::nullopt;
        }

        // Attach step to Background/Scenario; error if step before any Background:/Scenario:.
        inline std::optional<ParseOutcome> AttachParsedStep(ParsedFeature& feature,
                                                              std::optional<ParsedScenario>& currentScenario,
                                                              bool inBackground, std::size_t lineNo, ParsedStep step) {
            if (inBackground) {
                feature.background.push_back(std::move(step));
            } else if (currentScenario) {
                currentScenario->steps.push_back(std::move(step));
            } else {
                return MakeParseError(lineNo, "step found outside of a Background:/Scenario:");
            }
            return std::nullopt;
        }

        // Move in-progress Scenario to feature.scenarios (called between sections + end).
        inline void FlushScenario(ParsedFeature& feature, std::optional<ParsedScenario>& currentScenario) {
            if (currentScenario) {
                feature.scenarios.push_back(std::move(*currentScenario));
                currentScenario.reset();
            }
        }

        // Mutable parse state (one per ParseFeatureText call).
        struct FeatureParseState {
            ParsedFeature feature;
            std::vector<std::string> pendingTags;
            std::optional<ParsedScenario> currentScenario;
            bool inBackground = false;
            bool haveFeature = false;
        };

        // Classify trimmed Gherkin line; return error or std::nullopt (including
        // for free-text description lines). Per-line decision tree.
        inline std::optional<ParseOutcome> ProcessFeatureLine(FeatureParseState& state, std::string_view raw,
                                                                std::size_t lineNo) {
            const std::string_view trimmed = TrimView(raw);
            if (trimmed.empty() || trimmed.front() == '#') {
                return std::nullopt;
            }
            if (trimmed.front() == '@') {
                AppendTagsFromLine(trimmed, state.pendingTags);
                return std::nullopt;
            }
            if (const auto rejected = RejectIfUnsupportedConstruct(trimmed, lineNo)) {
                return rejected;
            }
            if (trimmed.starts_with("Feature:")) {
                return HandleFeatureLine(state.feature, state.haveFeature, state.pendingTags, trimmed, lineNo);
            }
            if (trimmed.starts_with("Background:")) {
                FlushScenario(state.feature, state.currentScenario);
                state.inBackground = true;
                state.pendingTags.clear(); // Background: does not take tags in this version
                return std::nullopt;
            }
            if (trimmed.starts_with("Scenario:") || trimmed.starts_with("Example:")) {
                FlushScenario(state.feature, state.currentScenario);
                state.inBackground = false;
                const std::size_t colonPos = trimmed.find(':');
                ParsedScenario scenario;
                scenario.name = std::string(TrimView(trimmed.substr(colonPos + 1)));
                scenario.tags = std::move(state.pendingTags);
                state.pendingTags.clear();
                scenario.line = lineNo;
                state.currentScenario = std::move(scenario);
                return std::nullopt;
            }
            if (const auto matched = MatchStepKeyword(trimmed)) {
                const auto& [keyword, rest] = *matched;
                ParsedStep step;
                step.keyword = keyword;
                step.text = std::string(TrimView(rest));
                step.line = lineNo;
                step.column = LeadingWhitespaceCount(raw) + 1;
                return AttachParsedStep(state.feature, state.currentScenario, state.inBackground, lineNo,
                                         std::move(step));
            }
            // Ignorable free-text description line.
            return std::nullopt;
        }

        // Free-text prose under Feature:/Scenario:/Background: ignored (no executable meaning).
        inline ParseOutcome ParseFeatureText(std::string_view text) {
            const std::vector<std::string_view> lines = SplitLines(text);
            FeatureParseState state;

            for (std::size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
                if (const auto failure = ProcessFeatureLine(state, lines[lineIdx], lineIdx + 1)) {
                    return *failure;
                }
            }
            FlushScenario(state.feature, state.currentScenario);
            if (!state.haveFeature) {
                return MakeParseError(0, "no 'Feature:' found");
            }
            ParseOutcome outcome;
            outcome.ok = true;
            outcome.feature = std::move(state.feature);
            return outcome;
        }

        inline std::string MakeFeatureLocation(std::string_view featureLabel, std::size_t line, std::size_t column) {
            return std::string(featureLabel) + ":" + std::to_string(line) + ":" + std::to_string(column);
        }

    } // namespace impl

    // Consumer-constructed registry of step definitions (Given/When/Then/And/But
    // with {int}/{float}/{string}/{word} placeholders) and Before/After hooks
    // (tag-filtered AND/subset). NOT a singleton; passed by reference to RunFeature.
    class StepRegistry {
    public:
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

        // --- Used internally by RunFeature()/RunScenario() below; not part
        // of the fluent registration API most consumers need directly. ---

        [[nodiscard]] std::optional<StepFunction> TryMatch(impl::StepKeyword keyword, const std::string& text) const {
            for (const auto& definition : m_definitions.at(static_cast<std::size_t>(keyword))) {
                std::smatch match;
                if (std::regex_match(text, match, definition.pattern.regex)) {
                    std::vector<std::string> captures;
                    captures.reserve(!match.empty() ? match.size() - 1 : 0);
                    for (std::size_t i = 1; i < match.size(); ++i) {
                        captures.emplace_back(match[i].str());
                    }
                    const auto thunk = definition.thunk;
                    return StepFunction([thunk, captures = std::move(captures)](TestContext& ctx) mutable -> bool {
                        return thunk(ctx, captures);
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

    private:
        template<typename F>
        void AddStepDefinition(impl::StepKeyword keyword, const std::string& pattern, F stepFn) {
            impl::CompiledStepPattern compiled = impl::CompileStepPattern(pattern);
            const std::size_t expectedArgs = impl::StepDefinitionArgCount<F>();
            if (compiled.placeholderCount != expectedArgs) {
                throw std::invalid_argument(
                    "BabyBehave::Gherkin: step pattern '" + pattern + "' declares " +
                    std::to_string(compiled.placeholderCount) + " placeholder(s) but its step definition takes " +
                    std::to_string(expectedArgs) + " parameter(s) after TestContext&");
            }
            m_definitions.at(static_cast<std::size_t>(keyword))
                .push_back(impl::StepDefinition{ .pattern = std::move(compiled),
                                                  .patternText = pattern,
                                                  .thunk = impl::MakeStepThunk(std::move(stepFn)) });
        }

        // One vector per impl::StepKeyword; And/But matched to own registered patterns.
        std::array<std::vector<impl::StepDefinition>, impl::kStepKeywordCount> m_definitions;
        std::vector<impl::Hook> m_beforeHooks;
        std::vector<impl::Hook> m_afterHooks;
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
    };

    namespace impl {

        // Build BabyBehaveTest step from Gherkin step: look up in registry,
        // or substitute synthetic failing step if no match. Always adds step.
        inline void AddParsedStepToTest(BabyBehaveTest& test, const ParsedStep& step, const StepRegistry& registry,
                                          std::string_view featureLabel, std::string_view namePrefix) {
            const std::string location = MakeFeatureLocation(featureLabel, step.line, step.column);
            std::string name = std::string(namePrefix) + step.text;
            std::optional<StepFunction> matched = registry.TryMatch(step.keyword, step.text);
            StepFunction stepFn = matched ? std::move(*matched)
                                       : StepFunction([text = step.text](TestContext&) -> bool {
                                             detail::PrintErrorLine(
                                                 "BabyBehave::Gherkin: no step definition matches: '" + text + "'");
                                             return false;
                                         });
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

        // Builds the same diagnostic text ReportScenarioFailureAndExit used to
        // print+exit directly; now returned so the caller can hand it to an
        // onFailure callback instead (which may or may not exit).
        inline std::string FormatScenarioFailureMessage(const TestResult& result) {
            std::string message = "BabyBehave::Gherkin: scenario '" + result.testName + "' failed:";
            for (const auto& step : result.steps) {
                if (step.passed) {
                    continue;
                }
                message += "\n  [" + step.stepLabel + "] " + step.stepName + ": " + step.message;
                if (!step.location.empty()) {
                    message += " (at " + step.location + ")";
                }
            }
            return message;
        }

        // Build and run BabyBehaveTest for scenario. Execution order:
        // Before hooks -> Background -> Scenario steps -> After hooks.
        inline TestResult RunScenario(const ParsedFeature& feature, const ParsedScenario& scenario,
                                        const std::vector<std::string>& effectiveTags, const StepRegistry& registry,
                                        std::string_view featureLabel, const GherkinFailureCallback& onFailure) {
            // suppressGivenNarration=true: synthetic no-op setup (suppresses redundant line).
            BabyBehaveTest test(scenario.name, [](TestContext&) {}, true);

            // Force collect-failures mode to guarantee After hooks run (reimplements
            // fail-hard below by inspecting TestResult::allPassed after Execute()).
            test.SetCollectFailuresMode(true);

            for (const auto& hook : registry.BeforeHooks()) {
                if (TagsAreSubsetOf(hook.tags, effectiveTags)) {
                    // WrapHookAsStep() called fresh per Scenario (Hook fn is copyable,
                    // but StepFunction is move-only).
                    test.AddStepAt<Precondition>("[Before] " + hook.label, WrapHookAsStep(hook.fn),
                                                    MakeFeatureLocation(featureLabel, scenario.line, 0));
                }
            }
            for (const auto& step : feature.background) {
                AddParsedStepToTest(test, step, registry, featureLabel, "[Background] ");
            }
            for (const auto& step : scenario.steps) {
                AddParsedStepToTest(test, step, registry, featureLabel, "");
            }
            for (const auto& hook : registry.AfterHooks()) {
                if (TagsAreSubsetOf(hook.tags, effectiveTags)) {
                    test.AddStepAt<Postcondition>("[After] " + hook.label, WrapHookAsStep(hook.fn),
                                                     MakeFeatureLocation(featureLabel, scenario.line, 0));
                }
            }

            const TestResult& result = test.Execute();
            if (!result.allPassed) {
                onFailure(FormatScenarioFailureMessage(result));
                // onFailure may exit/throw (default does); if it returns
                // normally, collect-failures mode was already forced above,
                // so simply continue and return result as-is - this is what
                // lets a non-exiting callback "collect Gherkin failures
                // across the whole feature" instead of stopping at the first.
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
    inline FeatureResult RunFeature(std::string_view featureText, StepRegistry& registry,
                                      std::string_view featureLabel = "<feature>",
                                      const GherkinFailureCallback& onFailure = impl::DefaultGherkinFailureAction) {
        const impl::ParseOutcome parsed = impl::ParseFeatureText(featureText);
        if (!parsed.ok) {
            onFailure(parsed.error);
            return FeatureResult{ .featureName = std::string(), .scenarioResults = {}, .allPassed = false };
        }

        FeatureResult result;
        result.featureName = parsed.feature.name;
        for (const auto& scenario : parsed.feature.scenarios) {
            const std::vector<std::string> effectiveTags = impl::UnionTags(parsed.feature.tags, scenario.tags);
            TestResult scenarioResult =
                impl::RunScenario(parsed.feature, scenario, effectiveTags, registry, featureLabel, onFailure);
            result.allPassed = result.allPassed && scenarioResult.allPassed;
            result.scenarioResults.push_back(std::move(scenarioResult));
        }
        return result;
    }

} // namespace Gherkin
#endif // BABYBEHAVE_DISABLE_GHERKIN

} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP