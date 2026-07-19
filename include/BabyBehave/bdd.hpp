#ifndef BABYBEHAVE_BDD_HPP
#define BABYBEHAVE_BDD_HPP

#pragma once

<<<<<<< HEAD
#include <algorithm>
=======
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
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
<<<<<<< HEAD
#include <cstdint>
=======
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
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


namespace BabyBehave::BDD {

    namespace detail {
        // Used in place of '\n'/std::endl everywhere in this file: cheaper
        // than std::endl (no forced flush - std::cerr is unit-buffered and
        // flushes on every insertion anyway, and std::cout's flush here is
        // no more necessary than for any other line), and named for
        // consistency with the string constants below it.
        inline constexpr char kNewLine = '\n';

        // Runtime toggle for whether PrintLine() below actually writes
        // anything. Defaults to enabled - unchanged behavior for every
        // existing consumer, and the whole point of self-hosted examples
        // like examples/SelfTest.cpp - but can be silenced, which matters
        // when a BabyBehaveTest runs inside a gtest suite: its step
        // narration otherwise interleaves with gtest's own "[ RUN ]"/
        // "[ OK ]" lines. Read once from the BABYBEHAVE_QUIET environment
        // variable (any non-empty value other than "0" disables) and
        // cached in this function-local static; BabyBehave::BDD::
        // SetNarrationEnabled() below can also flip it at runtime. CTest
        // sets BABYBEHAVE_QUIET=1 as a per-test environment variable on
        // the gtest-based suites (see tests/CMakeLists.txt), so running
        // one of those binaries directly - bypassing ctest - still shows
        // narration for debugging.
        inline bool& NarrationEnabledFlag() {
            static bool enabled = [] {
                const char* env = std::getenv("BABYBEHAVE_QUIET");
<<<<<<< HEAD
                return env == nullptr || *env == '\0' || std::string_view(env) == "0";
=======
                return env == nullptr || env[0] == '\0' || std::string_view(env) == "0";
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
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

        // Same toggle as PrintLine() above, for BabyBehave's own std::cerr
        // diagnostics (a missing TestContext key, a default failure
        // callback firing, a user callback itself throwing) - these are
        // just as noisy interleaved with gtest's output as the step
        // narration is, and gated the same way. Never suppresses the
        // exception/message data itself (e.g. TestContext::Get()'s thrown
        // std::out_of_range, or a death test's exit code): only this
        // extra, human-facing console line.
        inline void PrintErrorLine(const std::string& text) {
            if (!NarrationEnabledFlag()) {
                return;
            }
            std::cerr << text << kNewLine;
        }

        // Which text representation BabyBehaveTest's step narration uses.
        // Plain is the original, unstyled format (kGivenPrefix and friends
        // below) - live, printed one line at a time as each step runs,
        // byte-identical to every version of this file before this enum
        // existed. Arrow and Tree are opt-in, structured renderers (see
        // RenderArrow()/RenderTree() below): rather than printing
        // progressively, they buffer each step's kind and name as it runs
        // (see BabyBehaveTest::m_narrationSteps) and render the whole
        // scenario as one "[ OK ]"/"[ FAIL ] "-headed block once the
        // outcome is known - either at the natural end of Execute() (every
        // step passed, or a non-exiting callback absorbed every failure),
        // or never, if the true default (exit-on-failure) callback fires
        // first. That is a deliberate trade-off of choosing a buffered
        // style: Plain always shows which step it was on before a hang or
        // a hard exit; Arrow/Tree only show their summary if Execute()
        // gets to return.
<<<<<<< HEAD
        enum class NarrationStyle : std::uint8_t {
=======
        enum class NarrationStyle {
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
            Plain,
            Arrow,
            Tree,
        };

        // Maps BABYBEHAVE_STYLE's raw value to a NarrationStyle
        // ("plain"/"arrow"/"tree", case-sensitive; nullptr or anything
        // else defaults to Plain). Pulled out of NarrationStyleFlag()
        // below as a pure function - unlike NarrationEnabledFlag()'s
        // env-var branches (only two, and naturally exercised by
        // whichever ctest binaries do/don't set BABYBEHAVE_QUIET), this
        // has four distinct outcomes that would otherwise need their own
        // dedicated per-value process/environment just to reach for
        // coverage; as a pure function every case is one direct call.
        inline NarrationStyle ParseNarrationStyleEnv(const char* env) {
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

        // Read once from BABYBEHAVE_STYLE and cached, same pattern as
        // NarrationEnabledFlag() above. BabyBehave::BDD::
        // SetNarrationStyle() below can override it at runtime.
        inline NarrationStyle& NarrationStyleFlag() {
            static NarrationStyle style = ParseNarrationStyleEnv(std::getenv("BABYBEHAVE_STYLE"));
            return style;
        }

        // Identifies which fluent method produced a buffered narration
        // step (see BabyBehaveTest::m_narrationSteps), for the Arrow/Tree
        // renderers below. Given has no tag here: there is always exactly
        // one, it is never buffered (BabyBehaveTest::m_testName already
        // holds its name), and it roots the Tree renderer's layout
        // specially rather than appearing in the step list.
<<<<<<< HEAD
        enum class StepKindTag : std::uint8_t {
=======
        enum class StepKindTag {
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
            With,
            When,
            Then,
            And,
            Or,
            But,
        };

        // True for the "detail" steps (Precondition/And/Or/But) that the
        // Tree renderer nests one level under a primary step (With under
        // GIVEN; And/Or/But under THEN) and that the Arrow renderer marks
        // with "+"; false for the primary Given/When/Then steps, which
        // Tree always renders as top-level siblings and Arrow marks with
        // "->".
        inline bool IsNarrationDetailKind(StepKindTag kind) {
            return kind == StepKindTag::With || kind == StepKindTag::And ||
                   kind == StepKindTag::Or || kind == StepKindTag::But;
        }

        // std::unreachable() (rather than a trailing "return \"\";") after
        // an exhaustive switch over every StepKindTag enumerator: falling
        // off the end here is genuinely impossible, not just unhandled,
        // and a dead defensive return is an uncoverable gcov line with no
        // corresponding test that could ever legitimately reach it.
        inline std::string_view ArrowKindWord(StepKindTag kind) {
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

        inline std::string_view TreeKindLabel(StepKindTag kind) {
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

        // One step buffered by BabyBehaveTest::NarrateStep() for the
        // Arrow/Tree renderers - see NarrationStyle above for why this
        // buffers instead of printing immediately.
        struct NarrationStepEntry {
            StepKindTag kind;
            std::string name;
        };

        // Both callers below always push at least one line before calling
        // this, so lines is never empty here. Folds via std::accumulate
        // rather than a hand-written loop over a named std::string local:
        // the latter left a same-named local alive across the whole
        // function body, which GCC's --coverage instrumentation marks as
        // an unreachable-looking exception-cleanup line at the closing
        // brace (visible in gcov as "=====", counted as uncovered) even
        // though the function runs to completion on every call - the
        // fold's accumulator lives inside <numeric>'s template code
        // instead, outside what gcov attributes to this file.
        inline std::string JoinNarrationLines(const std::vector<std::string>& lines) {
            return std::accumulate(std::next(lines.begin()), lines.end(), lines.front(),
<<<<<<< HEAD
                                    [](const std::string& acc, const std::string& line) { return acc + kNewLine + line; });
=======
                                    [](std::string acc, const std::string& line) { return acc + kNewLine + line; });
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
        }

        // "-> Given a: X" / "    + With: Y" / "    -> When: Z" /
        // "    -> Then: W" / "    + And: V" - see NarrationStyle above for
        // the buffering model and IsNarrationDetailKind() for the "->" vs
        // "+" rule (Given/When/Then get "->"; With/And/Or/But get "+").
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

        // Left-justifies a Tree step label to a fixed field width so every
        // step name in the rendered block starts in the same column
        // (GIVEN/WITH/WHEN/THEN/AND/OR/BUT all fit within 7 characters).
        // Single return expression, no named local - see
        // JoinNarrationLines() above for why that matters for coverage.
        inline std::string PadTreeLabel(std::string_view label) {
            constexpr std::size_t kFieldWidth = 7;
            return label.size() < kFieldWidth ? std::string(label) + std::string(kFieldWidth - label.size(), ' ')
                                               : std::string(label);
        }

        // Box-drawing/check-mark glyphs, spelled as \u escapes rather than
        // literal UTF-8 source bytes: \u universal-character-names are
        // portable across editors/tools that don't preserve raw multibyte
        // source bytes, and are what MSVC's /utf-8 flag (see the root
        // CMakeLists.txt) needs to correctly encode into the execution
        // charset - a literal glyph in the source risks being misread there.
        inline constexpr std::string_view kTreeCheckMark = "\u2713";         // CHECK MARK
        inline constexpr std::string_view kTreeCrossMark = "\u2717";        // BALLOT X
        inline constexpr std::string_view kTreeVerticalBar = "\u2502";      // BOX DRAWINGS LIGHT VERTICAL
        inline constexpr std::string_view kTreeBranchMid = "\u251c\u2500 "; // VERTICAL-AND-RIGHT, HORIZONTAL, space
        inline constexpr std::string_view kTreeBranchLast = "\u2570\u2500 "; // ARC-UP-AND-RIGHT, HORIZONTAL, space

        // One top-level branch of the Tree renderer below, beyond GIVEN
        // itself: a WHEN step (never has children - nothing nests under
        // it) or a THEN step (its And/Or/But entries nest under it, see
        // RenderTree()). label/name point into the caller's data, not
        // owned here.
        struct TreeBranch {
            std::string_view label;
            const std::string* name;
            const std::vector<const NarrationStepEntry*>* children;
        };

<<<<<<< HEAD
        // Appends one indented, box-drawing-prefixed line per child entry
        // (WITH under GIVEN, or And/Or/But under a THEN branch) - the exact
        // same "last child gets the arc-corner glyph, everyone else gets
        // the tee glyph" pattern RenderTree() below needs at both nesting
        // points, factored out so it isn't duplicated (and so it doesn't
        // count twice toward RenderTree()'s own cognitive complexity).
        inline void AppendTreeChildLines(std::vector<std::string>& lines, std::string_view indent,
                                          const std::string& continuation,
                                          const std::vector<const NarrationStepEntry*>& children) {
            for (std::size_t i = 0; i < children.size(); ++i) {
                const bool lastChild = (i + 1 == children.size());
                lines.push_back(std::string(indent) + continuation +
                                 std::string(lastChild ? kTreeBranchLast : kTreeBranchMid) +
                                 PadTreeLabel(TreeKindLabel(children[i]->kind)) + children[i]->name);
            }
        }

=======
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
        // Unicode box-drawing tree: GIVEN, WHEN and THEN are always
        // top-level siblings (in that order); WITH nests one level under
        // GIVEN, and And/Or/But nest one level under THEN (under the
        // *last* THEN entry, if the caller buffered more than one - see
        // NarrationStyle above for the buffering model). If And/Or/But
        // steps were recorded with no THEN at all, they're promoted to
        // their own top-level branches instead of being dropped. Indent
        // is a fixed 2 spaces regardless of the "[ OK ]"/"[ FAIL ]"
        // header's width, so the tree body always lines up the same way.
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
<<<<<<< HEAD
            branches.reserve(whenEntries.size() + std::max(thenEntries.size(), thenChildren.size()));
            for (const auto* entry : whenEntries) {
                branches.push_back(
                    TreeBranch{ .label = TreeKindLabel(StepKindTag::When), .name = &entry->name, .children = &kNoChildren });
            }
            if (thenEntries.empty()) {
                for (const auto* entry : thenChildren) {
                    branches.push_back(
                        TreeBranch{ .label = TreeKindLabel(entry->kind), .name = &entry->name, .children = &kNoChildren });
=======
            for (const auto* entry : whenEntries) {
                branches.push_back(TreeBranch{ TreeKindLabel(StepKindTag::When), &entry->name, &kNoChildren });
            }
            if (thenEntries.empty()) {
                for (const auto* entry : thenChildren) {
                    branches.push_back(TreeBranch{ TreeKindLabel(entry->kind), &entry->name, &kNoChildren });
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
                }
            } else {
                for (std::size_t i = 0; i < thenEntries.size(); ++i) {
                    const bool isLastThen = (i + 1 == thenEntries.size());
<<<<<<< HEAD
                    branches.push_back(TreeBranch{ .label = TreeKindLabel(StepKindTag::Then),
                                                    .name = &thenEntries[i]->name,
                                                    .children = isLastThen ? &thenChildren : &kNoChildren });
=======
                    branches.push_back(TreeBranch{ TreeKindLabel(StepKindTag::Then), &thenEntries[i]->name,
                                                    isLastThen ? &thenChildren : &kNoChildren });
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
                }
            }
            const bool givenIsLastGroup = branches.empty();

            std::vector<std::string> lines;
            lines.push_back((passed ? "[ " + std::string(kTreeCheckMark) + " OK ] " : "[ " + std::string(kTreeCrossMark) + " FAIL ] ") + testName);
            lines.push_back(std::string(kIndent) + std::string(kTreeVerticalBar));
            lines.push_back(std::string(kIndent) + std::string(givenIsLastGroup ? kTreeBranchLast : kTreeBranchMid) + PadTreeLabel("GIVEN") + testName);

            const std::string givenChildContinuation = givenIsLastGroup ? "   " : std::string(kTreeVerticalBar) + "  ";
<<<<<<< HEAD
            AppendTreeChildLines(lines, kIndent, givenChildContinuation, givenChildren);
=======
            for (std::size_t i = 0; i < givenChildren.size(); ++i) {
                const bool lastChild = (i + 1 == givenChildren.size());
                lines.push_back(std::string(kIndent) + givenChildContinuation +
                                 std::string(lastChild ? kTreeBranchLast : kTreeBranchMid) +
                                 PadTreeLabel(TreeKindLabel(givenChildren[i]->kind)) + givenChildren[i]->name);
            }
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b

            for (std::size_t i = 0; i < branches.size(); ++i) {
                const bool lastBranch = (i + 1 == branches.size());
                lines.push_back(std::string(kIndent) + std::string(kTreeVerticalBar));
                lines.push_back(std::string(kIndent) + std::string(lastBranch ? kTreeBranchLast : kTreeBranchMid) +
                                 PadTreeLabel(branches[i].label) + *branches[i].name);

                const std::string branchChildContinuation = lastBranch ? "   " : std::string(kTreeVerticalBar) + "  ";
<<<<<<< HEAD
                AppendTreeChildLines(lines, kIndent, branchChildContinuation, *branches[i].children);
=======
                const auto& children = *branches[i].children;
                for (std::size_t c = 0; c < children.size(); ++c) {
                    const bool lastGrandchild = (c + 1 == children.size());
                    lines.push_back(std::string(kIndent) + branchChildContinuation +
                                     std::string(lastGrandchild ? kTreeBranchLast : kTreeBranchMid) +
                                     PadTreeLabel(TreeKindLabel(children[c]->kind)) + children[c]->name);
                }
>>>>>>> 867fc80e89b2260b872bed258b6cf1c0fbff5e4b
            }
            return JoinNarrationLines(lines);
        }

#if defined(__cpp_lib_source_location)
        // Formats a std::source_location as "file:line", for diagnostics
        // only (see StepResult::location and AddStep()'s source_location
        // default parameter below). Only compiled in when the standard
        // library actually provides <source_location>
        // (__cpp_lib_source_location); on toolchains without it, callers of
        // this function don't exist either, so nothing here is missed.
        inline std::string FormatLocation(const std::source_location& loc) {
            return std::string(loc.file_name()) + ":" + std::to_string(loc.line());
        }
#endif

        // Step-type labels shared by BabyBehaveTest::executeStep()'s branches
        // and by Execute()'s context-setup handling below: each is passed as
        // BOTH the StepResult::stepLabel and (for Precondition/Action/
        // Postcondition/ContextSetup) the exception-callback "step"
        // description, at up to five call sites per branch. Named here once
        // so renaming a step type touches a single line instead of every one
        // of those call sites.
        inline constexpr std::string_view kPreconditionLabel = "Precondition";
        inline constexpr std::string_view kActionLabel = "Action";
        inline constexpr std::string_view kPostconditionLabel = "Postcondition";
        inline constexpr std::string_view kAndLabel = "And";
        inline constexpr std::string_view kOrLabel = "Or";
        inline constexpr std::string_view kButLabel = "But";
        inline constexpr std::string_view kContextSetupLabel = "ContextSetup";

        // Message for the synthesized std::runtime_error that stands in for
        // a caught `...` (non-std::exception) in every catch-all block below
        // (context setup and all six step branches) - identical at every one
        // of those call sites.
        inline constexpr std::string_view kUnknownExceptionMessage = "unknown non-std::exception type thrown";

        // Prefix shared by both context-setup failure paths in Execute()
        // below (the std::exception catch and the catch-all).
        inline constexpr std::string_view kContextSetupExceptionPrefix = "Exception caught in Context Setup: ";

        // Every other user-facing string literal in this header, gathered
        // here for consistency with the labels/messages above even though
        // each is used at exactly one call site - so every piece of
        // BabyBehave's own console/diagnostic text lives in one place
        // rather than some of it here and some of it inline.
        inline constexpr std::string_view kKeyNotFoundPrefix = "Key not found: ";
        inline constexpr std::string_view kGivenPrefix = "Given a: ";
        inline constexpr std::string_view kWithPrefix = "    With: ";
        inline constexpr std::string_view kWhenPrefix = "    When: ";
        inline constexpr std::string_view kThenPrefix = "    Then: ";
        inline constexpr std::string_view kAndPrefix = "    And: ";
        inline constexpr std::string_view kOrPrefix = "    Or: ";
        inline constexpr std::string_view kButPrefix = "    But: ";
        // Prefix for BabyBehaveTest's genuine default onException callback
        // (InitDefaultCallbacks() below); NOT related to
        // kContextSetupExceptionPrefix above, which is for context-setup
        // failures specifically, not the default exception callback.
        inline constexpr std::string_view kDefaultExceptionCallbackPrefix = "Exception caught in ";
        inline constexpr std::string_view kOnConditionNotVerifiedCallbackThrewMessage =
            "BabyBehave: onConditionNotVerified callback itself threw an exception; ignoring to avoid std::terminate()";
        inline constexpr std::string_view kOnExceptionCallbackThrewMessage =
            "BabyBehave: onException callback itself threw an exception; ignoring to avoid std::terminate()";

        // Fully-formed, per-step-type failure messages/exception-step
        // descriptions, precomputed as compile-time literals rather than
        // concatenated at runtime from the labels above: executeStep() below
        // passes these as std::string_view straight through to
        // VerifyCondition()/SafeInvokeExceptionCallback(), so a PASSING step
        // (the common case) allocates nothing for them at all - they are
        // only ever materialized into an owning std::string on the failure
        // path, inside those two functions, exactly where one is needed.
        //
        // One group per step type, in the same order executeStep() below
        // checks them (Precondition, Action, Postcondition, And, Or, But),
        // so each group can be read side by side with its branch there.
        // Precondition/Action/Postcondition only need a failure message:
        // their exception-callback "step" description reuses the plain
        // kPreconditionLabel/kActionLabel/kPostconditionLabel from above
        // as-is. And/Or/But additionally need a *ConditionLabel constant,
        // because - unlike the first three - their exception-callback "step"
        // description is "And"/"Or"/"But" + " condition", not the bare
        // label (see executeStep()'s SafeInvokeExceptionCallback() calls).

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

        // Appends a " (at file:line)" suffix to `text` when `location` is
        // non-empty (i.e. this build has <source_location> support and the
        // step/context-setup call site was captured); returns `text`
        // unchanged otherwise. Shared by VerifyCondition() and
        // SafeInvokeExceptionCallback() below, which both append the same
        // suffix format to a failure/exception message.
        inline std::string AppendLocationSuffix(const std::string& text, const std::string& location) {
            return location.empty() ? text : (text + " (at " + location + ")");
        }

        // Transparent (C++20 heterogeneous-lookup) hash for
        // TestContext::m_objects below, paired with std::equal_to<> (itself
        // already transparent). Lets TestContext::Get() look a key up via
        // find(std::string_view) without first materializing an owning
        // std::string just to satisfy unordered_map's key_type - which
        // matters because ContextKey<T>-based Get() calls (the common,
        // compile-time-checked path) previously paid that allocation on
        // every single lookup. Set() still needs an actual std::string to
        // insert, so it is unaffected (see its comment below).
        struct TransparentStringHash {
            using is_transparent = void;
            std::size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
        };
    } // namespace detail

    // Enables/disables BabyBehaveTest's step narration ("Given a: .../
    // With: .../When: .../Then: ...", printed as each step runs). Process-
    // wide and takes effect immediately (including for scenarios already
    // under construction). Defaults to enabled unless BABYBEHAVE_QUIET was
    // set before the first print - see detail::NarrationEnabledFlag()
    // above for the full rationale (mainly: silencing this inside a gtest
    // suite, where it would otherwise interleave with gtest's own output).
    inline void SetNarrationEnabled(bool enabled) {
        detail::NarrationEnabledFlag() = enabled;
    }

    // Publicly-spelled alias for detail::NarrationStyle (defined next to
    // detail::PrintLine()/PrintErrorLine() above, which is where the
    // Plain/Arrow/Tree formats and their live-vs-buffered trade-off are
    // documented) - callers write BabyBehave::BDD::NarrationStyle::Tree,
    // not the detail:: form.
    using NarrationStyle = detail::NarrationStyle;

    // Selects which text representation BabyBehaveTest's step narration
    // uses (Plain/Arrow/Tree). Process-wide, takes effect immediately, and
    // only matters while narration is enabled (see SetNarrationEnabled()
    // above) - see NarrationStyle above for what each style looks like and
    // the live-vs-buffered trade-off between them.
    inline void SetNarrationStyle(NarrationStyle style) {
        detail::NarrationStyleFlag() = style;
    }

    // Outcome of one named sub-check recorded via SoftCheck::Check() (see
    // SoftCheck below). label/message are exactly what was passed to
    // Check(); passed is the condition that was recorded for it.
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

        // Side-channel used by the optional SoftCheck recorder (see below)
        // to get named sub-check results from inside a step body back out
        // to BabyBehaveTest, despite StepFunction being a plain
        // bool(TestContext&) with no other output channel. A SoftCheck is
        // typically a step-local stack variable that is destroyed when the
        // step function returns - before BabyBehaveTest::executeStep() gets
        // a chance to inspect anything - so results are written straight
        // through to the owning TestContext as each Check() call happens,
        // rather than being kept only inside the (soon to be destroyed)
        // SoftCheck object. This is deliberately NOT thread-local: it is a
        // plain member tied to this TestContext instance, which is
        // consistent with TestContext already being documented as not
        // thread-safe, and with BabyBehaveTest::Execute() running every
        // step strictly sequentially - so there is never more than one step
        // writing to a given TestContext's soft-check results at a time.
        // BabyBehaveTest clears this before each step runs (see
        // ClearSoftCheckResults()) so results never leak from one step into
        // the next.
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
        // key is taken by const& (not string_view): unordered_map::operator[]
        // has no heterogeneous overload before C++26, so inserting/
        // overwriting always needs an actual std::string key_type argument
        // either way - taking string_view here would just move that
        // construction from the call site into this function for no
        // savings, and would cost an extra allocation on the (very common)
        // overwrite-existing-key case, where operator[] currently needs zero
        // key copies at all when `key` is already a std::string lvalue.
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = std::move(obj);
        }

        // key is std::string_view (rather than const std::string&) so a
        // lookup - unlike Set() above - never has to materialize an owning
        // std::string just to search: find() uses TransparentStringHash +
        // std::equal_to<> above to hash/compare the view directly against
        // the map's std::string keys.
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

        // Opt-in, compile-time-checked key for TestContext. Declare one instance per
        // logical value, e.g.
        //   inline constexpr BabyBehave::BDD::TestContext::ContextKey<std::shared_ptr<AlarmClock>>
        //       kAlarmClock{"AlarmClock"};
        // and then use context.Set(kAlarmClock, ...) / context.Get(kAlarmClock) to get a
        // compile error on type mismatches instead of a runtime std::bad_any_cast.
        // Internally it just delegates to the existing string-keyed storage above, so it
        // is fully interoperable with the plain string-based Set<T>/Get<T> API.
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

    // Optional, opt-in recorder that lets a step body report MULTIPLE named
    // sub-checks instead of a single overall pass/fail, without changing
    // StepFunction's bool(TestContext&) signature. Typical use inside a
    // step:
    //
    //   bool MyStep(TestContext& context) {
    //       SoftCheck checks(context);
    //       checks.Check("has valid id", someId > 0);
    //       checks.Check("name matches", name == "expected");
    //       checks.Check("count in range", count >= 1 && count <= 10,
    //                     "count was " + std::to_string(count));
    //       return checks.AllPassed();
    //   }
    //
    // This is a simple recorder, not a lazy-assertion DSL: `condition` is a
    // plain bool evaluated eagerly by the caller before Check() ever sees
    // it, so it composes fine with raw comparisons or with
    // BabyBehave::Matchers::Expect(...).ToXxx(...) - SoftCheck only adds
    // naming/grouping on top, it does not compute or interpret the checks
    // itself.
    //
    // A step that never constructs a SoftCheck, or constructs one but never
    // calls Check(), is completely unaffected: AllPassed() defaults to
    // true, and BabyBehaveTest only looks at a step's recorded sub-checks
    // when that step's own bool comes back false AND at least one sub-check
    // was actually recorded as failed (see
    // BabyBehaveTest::FormatFailedSoftChecks()) - so this feature is purely
    // additive and never changes the behavior/output of a step that doesn't
    // use it.
    class SoftCheck {
    public:
        explicit SoftCheck(TestContext& context) : m_context(context) {}

        // Records one named sub-check and returns `condition` unchanged
        // (purely for caller convenience, e.g. early-return patterns).
        // label/message are sink parameters (taken by value, moved into the
        // recorded SoftCheckResult) so a caller passing a literal or a
        // disposable local pays exactly one string construction instead of
        // one-to-bind-the-reference-plus-one-to-copy-into-the-result.
        bool Check(std::string label, bool condition, std::string message = "") {
            m_context.RecordSoftCheck(SoftCheckResult{ .label = std::move(label), .passed = condition, .message = std::move(message) });
            if (!condition) {
                m_allPassed = false;
            }
            return condition;
        }

        // AND of every Check() call made so far; true if Check() was never
        // called, so a step that only conditionally records sub-checks
        // still behaves sensibly.
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

    // Named here (rather than spelled out at both SetOnConditionNotVerifiedCallback()/
    // SetOnExceptionCallback()'s parameter and the matching m_on...Callback
    // member below) for the same reason as StepFunction/ContextSetupFunction
    // above: one signature to update instead of two staying in sync by hand.
    // Plain std::function (not move_only_function): unlike StepFunction/
    // ContextSetupFunction, consumer callbacks are invoked repeatedly (once
    // per failing step) rather than consumed once, and BabyBehaveTest never
    // needs to move one out of itself.
    using ConditionNotVerifiedCallback = std::function<void(const std::string& errorMsg)>;
    using ExceptionCallback = std::function<void(const std::string& step, const std::exception&)>;

    struct Precondition { StepFunction fn; };
    struct Action { StepFunction fn; };
    struct Postcondition { StepFunction fn; };
    struct And { StepFunction fn; };
    struct Or { StepFunction fn; };
    struct But { StepFunction fn; };

    // Outcome of a single executed step, populated only when a
    // BabyBehaveTest has opted into SetCollectFailuresMode(true). stepLabel
    // is the step type ("Precondition", "Action", "Postcondition", "And",
    // "Or", "But"); stepName is the name passed to AddStep (i.e. the
    // function name captured by the Given/With/When/... macros). message is
    // empty when passed is true, and holds the failure/exception message
    // otherwise. location is the "file:line" of the AddStep()/With()/
    // When()/... call site that registered this step, captured
    // automatically via std::source_location when the standard library
    // supports it (__cpp_lib_source_location); it is populated for every
    // step (pass or fail) in that case, and is always the empty string on
    // toolchains without <source_location> support.
    struct StepResult {
        std::string stepLabel;
        std::string stepName;
        bool passed = true;
        std::string message;
        std::string location;
    };

    // Outcome of an entire scenario. allPassed is the logical AND of every
    // recorded StepResult::passed (trivially true if no steps were
    // recorded, e.g. collect-failures mode was never enabled).
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
        // The source_location parameter defaults to the CALLER's location
        // (std::source_location::current() as a default argument is
        // specified to capture the call site, not this constructor's own
        // body) - so a direct `BabyBehaveTest(name, setupFn)` call captures
        // its own call site, and GivenAImpl() below instead forwards the
        // location IT captured (i.e. the Given/GivenA macro's call site) so
        // that a context-setup failure is attributed to the scenario's
        // Given(...) line, not to GivenAImpl's internal constructor call.
        // testName is a sink parameter (taken by value, moved into
        // m_testName): callers only ever pass a string literal via the
        // Given/GivenA macros' #func stringification, or forward one along
        // from GivenAImpl() below - either way there is exactly one owning
        // std::string to construct, and taking testName by const& here
        // would force a second copy on top of that instead of a move.
        BabyBehaveTest(std::string testName, ContextSetupFunction contextSetupFn,
                        const std::source_location& loc = std::source_location::current())
            : m_testName(std::move(testName)),
            m_contextSetupFn(std::move(contextSetupFn)),
            m_contextSetupLocation(detail::FormatLocation(loc)) {
            InitDefaultCallbacks();
        }
#else
        BabyBehaveTest(std::string testName, ContextSetupFunction contextSetupFn)
            : m_testName(std::move(testName)),
            m_contextSetupFn(std::move(contextSetupFn)) {
            InitDefaultCallbacks();
        }
#endif

        // Only allocation failure (std::bad_alloc, e.g. from the
        // std::string/std::vector bookkeeping in Execute()/VerifyCondition()
        // below) could theoretically escape this destructor and trigger
        // std::terminate() - every step/context-setup/callback exception is
        // deliberately caught and routed internally (see Execute() and
        // VerifyCondition()/SafeInvokeExceptionCallback() below). That
        // residual risk is accepted, not guarded against, the same way
        // AppendFailedSoftChecks() above doesn't guard its own string
        // concatenation against bad_alloc: defending against allocation
        // failure throughout would add complexity for a case no realistic
        // test can exercise.
        // NOLINTNEXTLINE(bugprone-exception-escape)
        ~BabyBehaveTest() {
            // Execute() is idempotent (guarded by m_executed), so this is a
            // no-op if a consumer already called it manually (see Execute()
            // below for why/when one would do that).
            Execute();
        }

        BabyBehaveTest(const BabyBehaveTest&) = delete;
        BabyBehaveTest& operator=(const BabyBehaveTest&) = delete;

        // Move is deleted too, not just copy - not merely for symmetry, but
        // because a naive defaulted move would be actively wrong here:
        // ~BabyBehaveTest() runs Execute() (above), which is idempotent only
        // because it sets m_executed = true on entry; a defaulted move
        // constructor would copy that same bool into both the moved-to
        // object and the moved-from husk, so if the move happened before
        // Execute() ran, BOTH destructors would call it - once for real,
        // once again against the husk's already-moved-out (empty) steps and
        // context. Doing this correctly would mean hand-writing every
        // member's transfer plus marking the source "already executed" in
        // the constructor, and separately deciding what move-assignment
        // should do with *this's own not-yet-executed scenario (if any)
        // before overwriting it - solvable, but not warranted for a type
        // that's meant to be used as a single fluent chain expression
        // (Given(...).With(...)....;), never stored in a container or moved
        // around. GivenAImpl() below still returns BabyBehaveTest by value
        // despite this: C++17 guarantees copy elision for a prvalue
        // returned directly, so no move/copy constructor call is involved
        // there at all.
        BabyBehaveTest(BabyBehaveTest&&) = delete;
        BabyBehaveTest& operator=(BabyBehaveTest&&) = delete;

        void SetOnConditionNotVerifiedCallback(ConditionNotVerifiedCallback callback) {
            m_onConditionNotVerifiedCallback = std::move(callback);
        }

        void SetOnExceptionCallback(ExceptionCallback callback) {
            m_onExceptionCallback = std::move(callback);
        }

        // Opt-in: when enabled, a failed condition or a caught exception in
        // any step no longer invokes m_onConditionNotVerifiedCallback /
        // m_onExceptionCallback (which default to std::cerr + std::exit()).
        // Instead the outcome is recorded into an internal TestResult and
        // execution CONTINUES with the next step. Off by default, so any
        // consumer who does not call this sees byte-identical behavior to
        // before this feature existed. Fluent, like AddStep, so it chains:
        //   test.SetCollectFailuresMode().With(...).When(...).Then(...);
        BabyBehaveTest& SetCollectFailuresMode(bool enabled = true) {
            m_collectFailures = enabled;
            return *this;
        }

#if defined(__cpp_lib_source_location)
        // loc defaults to std::source_location::current(), which - per the
        // standard - captures the CALLER's location when used as a default
        // argument value. The Given/With/When/Then/And/Or/But macros expand
        // to a direct AddStep<StepType>(#func, {func}) call on the same
        // source line as the macro invocation, so this naturally resolves
        // to the macro call site with zero changes needed on the macros'
        // part; direct AddStep<...>(...) calls (see NoShortMacros.cpp) get
        // their own call site the same way.
        // name is a sink parameter for the same reason as testName above
        // (AddStep is called once per With/When/Then/And/Or/But, so this
        // saves one string copy per step registration).
        template<typename StepType>
        BabyBehaveTest& AddStep(std::string name, StepFunction stepFunction,
                                 const std::source_location& loc = std::source_location::current()) {
            StepVariant step = StepType{ std::move(stepFunction) };
            m_steps.emplace_back(std::move(name), std::move(step));
            m_stepLocations.push_back(detail::FormatLocation(loc));
            return *this;
        }
#else
        template<typename StepType>
        BabyBehaveTest& AddStep(std::string name, StepFunction stepFunction) {
            StepVariant step = StepType{ std::move(stepFunction) };
            m_steps.emplace_back(std::move(name), std::move(step));
            m_stepLocations.push_back(std::string());
            return *this;
        }
#endif

        const std::vector<Step>& GetSteps() const {
            return m_steps;
        }

        // Runs the scenario (context setup + every added step) and returns
        // the recorded TestResult. Idempotent: the first call executes and
        // caches the result, later calls (including the one implicitly made
        // by ~BabyBehaveTest()) just return the cached result.
        //
        // Execute() normally never needs to be called explicitly - the
        // destructor does it - but BabyBehaveTest runs from the destructor,
        // and the object is gone by the time the destructor returns, so a
        // consumer who wants the TestResult (i.e. who called
        // SetCollectFailuresMode(true)) must bind the test to a named
        // variable and call Execute() manually before it goes out of scope:
        //
        //   BabyBehaveTest test = GivenA(SetupContext);
        //   test.SetCollectFailuresMode(true);
        //   test.With(...).When(...).Then(...);
        //   const TestResult& result = test.Execute();
        //   // ... inspect result.allPassed / result.steps here ...
        //   // test's destructor is now a no-op, since m_executed is set.
        const TestResult& Execute() {
            if (m_executed) {
                return m_result;
            }
            m_executed = true;

            // Plain prints Given immediately, like every step below; Arrow/
            // Tree need nothing here (m_testName already holds Given's name
            // - see RenderArrow()/RenderTree()'s callers below).
            if (detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
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

            // Plain's trailing blank line, unchanged; Arrow/Tree instead
            // render everything buffered above as one block, now that the
            // outcome (m_anyStepFailedForNarration) is known - see
            // NarrationStyle's comment for why this point in Execute() is
            // the only place that happens (a real exit-on-failure default
            // callback never lets control reach here). Either way, a
            // trailing blank line follows the block: without it, one
            // scenario's Tree/Arrow output runs straight into the next
            // scenario's "[ OK ]"/"[ FAIL ]" header with nothing marking
            // where one ends and the other begins.
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

        // Returns the TestResult recorded so far. Only meaningful once
        // Execute() has run (manually, or via the destructor); before that
        // it is a default-constructed TestResult (empty testName, no
        // steps). Most useful together with SetCollectFailuresMode(true)
        // and a manual Execute() call - see Execute()'s comment above.
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

        // Called by each executeStep() branch below as its step begins.
        // Plain prints plainPrefix + name immediately, exactly as every
        // version of this file before NarrationStyle existed; Arrow/Tree
        // instead buffer (kind, name) into m_narrationSteps for
        // RenderArrow()/RenderTree() to consume once Execute() knows the
        // outcome (see NarrationStyle's comment for why that's later, not
        // here).
        void NarrateStep(detail::StepKindTag kind, std::string_view plainPrefix, const std::string& name) {
            if (detail::NarrationStyleFlag() == detail::NarrationStyle::Plain) {
                detail::PrintLine(std::string(plainPrefix) + name);
            } else {
                m_narrationSteps.push_back(detail::NarrationStepEntry{ .kind = kind, .name = name });
            }
        }

        // High cognitive-complexity score is inherent, not accidental: this
        // is six near-identical if-constexpr branches (one per StepType in
        // StepVariant), each doing the same try/VerifyCondition/catch/
        // SafeInvokeExceptionCallback dance with that branch's own
        // detail::k*Label/k*FailedMessage constants. Splitting it into six
        // separate functions wouldn't reduce the real complexity, just
        // scatter it and duplicate the std::visit dispatch site below.
        template<typename T>
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        void executeStep(const std::string& name, T& step, const std::string& location) {
            // Cleared here (once, regardless of step type) rather than
            // inside each branch below: every branch calls step.fn(m_context)
            // exactly once, so this guarantees a fresh, empty
            // SoftCheckResult list for that single call, and that results
            // never leak from one step into the next. See
            // TestContext::m_softCheckResults' comment for why this lives on
            // TestContext instead of e.g. thread-local storage.
            m_context.ClearSoftCheckResults();
            // Every argument below is a compile-time std::string_view
            // constant (see the detail namespace above) rather than a
            // per-call std::string, all the way down into VerifyCondition()/
            // SafeInvokeExceptionCallback(): a step that PASSES - the common
            // case - now allocates nothing at all for its labels/messages,
            // where every prior version of this function built at least one
            // (and, for And/Or/But, two) throwaway std::string objects per
            // step regardless of outcome.
            if constexpr (std::is_same_v<T, Precondition>) {
                NarrateStep(detail::StepKindTag::With, detail::kWithPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kPreconditionFailedMessage, detail::kPreconditionLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kPreconditionLabel, e, detail::kPreconditionLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kPreconditionLabel, unknownEx, detail::kPreconditionLabel, name, location);
                }
            } else if constexpr (std::is_same_v<T, Action>) {
                NarrateStep(detail::StepKindTag::When, detail::kWhenPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kActionFailedMessage, detail::kActionLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kActionLabel, e, detail::kActionLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kActionLabel, unknownEx, detail::kActionLabel, name, location);
                }
            } else if constexpr (std::is_same_v<T, Postcondition>) {
                NarrateStep(detail::StepKindTag::Then, detail::kThenPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kPostconditionFailedMessage, detail::kPostconditionLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kPostconditionLabel, e, detail::kPostconditionLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kPostconditionLabel, unknownEx, detail::kPostconditionLabel, name, location);
                }
            } else if constexpr (std::is_same_v<T, And>) {
                NarrateStep(detail::StepKindTag::And, detail::kAndPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kAndConditionFailedMessage, detail::kAndLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kAndConditionLabel, e, detail::kAndLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kAndConditionLabel, unknownEx, detail::kAndLabel, name, location);
                }
            } else if constexpr (std::is_same_v<T, Or>) {
                NarrateStep(detail::StepKindTag::Or, detail::kOrPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kOrConditionFailedMessage, detail::kOrLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kOrConditionLabel, e, detail::kOrLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kOrConditionLabel, unknownEx, detail::kOrLabel, name, location);
                }
            } else if constexpr (std::is_same_v<T, But>) {
                NarrateStep(detail::StepKindTag::But, detail::kButPrefix, name);
                try {
                    VerifyCondition(step.fn(m_context), detail::kButConditionFailedMessage, detail::kButLabel, name, location);
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback(detail::kButConditionLabel, e, detail::kButLabel, name, location);
                }
                catch (...) {
                    const std::runtime_error unknownEx{std::string(detail::kUnknownExceptionMessage)};
                    SafeInvokeExceptionCallback(detail::kButConditionLabel, unknownEx, detail::kButLabel, name, location);
                }
            } else {
                // static_assert's message must be a literal token in C++23
                // (only C++26 allows an arbitrary constant expression here),
                // so this one can't be moved into the detail namespace like
                // every other string literal in this header.
                static_assert(!sizeof(T), "Unknown step type");
            }
        }

        // If the step that just ran (via executeStep(), which clears
        // TestContext's soft-check results before every single step.fn()
        // call - see there) recorded any FAILED SoftCheck sub-checks, joins
        // their "label (message)" (or just "label" when message is empty)
        // with "; " for use as a message suffix; returns an empty string
        // otherwise (no SoftCheck used, or all recorded sub-checks passed -
        // e.g. the step's overall bool went false for some other reason).
        // Passing sub-checks are intentionally omitted: only the specific
        // failing ones are useful in a failure message.
        //
        // Appends directly onto the caller's `out` (rather than building
        // and returning its own std::string) so that when at least one
        // SoftCheck failed, VerifyCondition() gets its "errorMsg: ..."
        // suffix with no extra string construction/move beyond what it
        // already pays for `out` itself - and, incidentally, sidesteps a
        // gcov quirk where a same-shaped function ending in a bare
        // `return locallyBuiltString;` gets its closing brace permanently
        // flagged as an uncovered exception-unwind-only block (a landing
        // pad for destroying that local on the throwing-`bad_alloc` path),
        // even when the function itself is fully exercised.
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

        // Shared by every step branch above (and by context setup, with
        // stepLabel="ContextSetup"). When collect-failures mode is off
        // (default), behaves exactly as before this feature existed: only
        // invokes m_onConditionNotVerifiedCallback, and only when condition
        // is false (except that, when location is non-empty, a
        // " (at file:line)" suffix identifying the AddStep()/Given() call
        // site is now appended to the message passed to the callback).
        // When collect-failures mode is on, the callback is skipped
        // entirely (it could itself std::exit(), defeating the "keep
        // going" point of the mode) and the outcome is recorded into
        // m_result instead - for every step, not just failures, so
        // TestResult::steps reflects the full, in-order execution history;
        // location is recorded as-is (never suffixed onto message) via
        // StepResult::location.
        //
        // In both branches, when condition is false, errorMsg is first
        // augmented with ": <failed sub-check 1>; <failed sub-check 2>; ..."
        // (via FormatFailedSoftChecks() above) IF the step used the opt-in
        // SoftCheck recorder and recorded at least one failed sub-check;
        // otherwise errorMsg is passed through completely untouched, so a
        // step that doesn't use SoftCheck produces byte-identical messages
        // to before this feature existed.
        //
        // errorMsg/stepLabel are std::string_view (executeStep() above
        // passes the detail namespace's compile-time constants straight
        // through): when condition is true - the common case for a healthy
        // test suite - this function returns having built no std::string at
        // all beyond the (already-required) empty StepResult::message in
        // collect-failures mode. All the message-building below only runs
        // once we already know the step failed.
        void VerifyCondition(bool condition, std::string_view errorMsg,
                              std::string_view stepLabel, const std::string& stepName,
                              const std::string& location) {
            if (condition) {
                if (m_collectFailures) {
                    m_result.steps.push_back(StepResult{ .stepLabel = std::string(stepLabel), .stepName = stepName, .passed = true, .message = std::string(), .location = location });
                }
                return;
            }
            // Every path below this point is a failure, regardless of
            // collect-failures mode - RenderArrow()/RenderTree() read this
            // once Execute() reaches its end to decide "[ OK ]" vs
            // "[ FAIL ]" (see NarrationStyle's comment for why Plain
            // doesn't need it: it never shows a header at all).
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

        // See VerifyCondition() above for the collect-failures interaction,
        // the location suffix behavior, and why step/stepLabel are
        // std::string_view; the same reasoning applies here for
        // m_onExceptionCallback, except the suffix is appended to a copy of
        // the exception's message (via a fresh std::runtime_error carrying
        // `what() + " (at file:line)"`) rather than to a plain std::string,
        // since the callback's second parameter is a std::exception - the
        // "step" label (first parameter) is converted to std::string only
        // right at the callback invocation, which is as far as it can be
        // deferred (m_onExceptionCallback's signature requires a
        // std::string, not a string_view, for backward compatibility with
        // existing consumers keying off it).
        void SafeInvokeExceptionCallback(std::string_view step, const std::exception& e,
                                          std::string_view stepLabel, const std::string& stepName,
                                          const std::string& location) {
            // Always a failure - only reached from a caught exception - so
            // this is unconditional, unlike VerifyCondition()'s flag-set
            // above which only covers its own failure branch.
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
        TestContext m_context;
        std::vector<Step> m_steps;
        std::vector<std::string> m_stepLocations;

        // Populated only for the Arrow/Tree narration styles (see
        // NarrateStep() above); stays empty under Plain, which never reads
        // them. m_anyStepFailedForNarration mirrors m_result.allPassed but
        // is tracked separately since collect-failures mode still needs a
        // narration outcome even when it suppresses the exit-on-failure
        // callback that would otherwise stop Execute() from finishing.
        std::vector<detail::NarrationStepEntry> m_narrationSteps;
        bool m_anyStepFailedForNarration = false;

        ConditionNotVerifiedCallback m_onConditionNotVerifiedCallback;
        ExceptionCallback m_onExceptionCallback;

        bool m_collectFailures = false;
        bool m_executed = false;
        TestResult m_result;
    };


#if defined(__cpp_lib_source_location)
    // loc captures the Given(func)/GivenA(func) macro's call site (see the
    // BabyBehaveTest constructor's comment above for why it's forwarded
    // explicitly here rather than left to that constructor's own default).
    inline BabyBehaveTest GivenAImpl(const std::string& testName,
        ContextSetupFunction contextSetup,
        const std::source_location& loc = std::source_location::current()) {
        return { testName, std::move(contextSetup), loc };
    }
#else
    inline BabyBehaveTest GivenAImpl(const std::string& testName,
        ContextSetupFunction contextSetup) {
        return { testName, std::move(contextSetup) };
    }
#endif

    // Short, generic macro aliases (Given/When/Then/And/Or/But/...) for AddStep<>/GivenAImpl.
    // Define BABYBEHAVE_NO_SHORT_MACROS before including this header to skip them if they collide
    // with your own or another library's names; call AddStep<StepType>(...) / GivenAImpl(...) instead.
#ifndef BABYBEHAVE_NO_SHORT_MACROS
#define Given(func)  GivenAImpl(#func, {func})
#define GivenA(func) GivenAImpl(#func, {func})
#define With(func)  AddStep<Precondition>(#func, {func})
#define WithI(func) AddStep<Precondition>(#func, {func})
#define When(func)  AddStep<Action>(#func, {func})
#define WhenI(func) AddStep<Action>(#func, {func})
#define Then(func)  AddStep<Postcondition>(#func, {func})
#define ThenI(func) AddStep<Postcondition>(#func, {func})
#define And(func)   AddStep<And>(#func, {func})
#define AndI(func)  AddStep<And>(#func, {func})
#define But(func)   AddStep<But>(#func, {func})
#define ButI(func)  AddStep<But>(#func, {func})
#define Or(func)    AddStep<Or>(#func, {func})
#define OrI(func)   AddStep<Or>(#func, {func})
#endif // BABYBEHAVE_NO_SHORT_MACROS
} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP
