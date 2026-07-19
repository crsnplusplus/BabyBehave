// SelfTestDiagnostics.hpp
//
// Shared scaffolding for the BBH self-hosted coverage binaries
// (test_SelfTest.cpp and test_SelfTest_ExceptionCoverage.cpp): a
// ReportScenario() pair, the Flat/Tree "Expected X / <narration> /
// TEST Y" mismatch-diagnostic formatters, and the --style=/--color=/
// --diagnostics= CLI parsing that drives them.
//
// This used to be duplicated per-TU (each self-test binary keeping its own
// copy, per each file's "separate, self-contained TU" comment) but that let
// test_SelfTest_ExceptionCoverage.cpp's copy silently fall behind
// test_SelfTest.cpp's - it kept the old plain "[OK]/[FAIL]" bool overload
// with no captured-narration diagnostics at all, so every one of its
// scenarios always printed BabyBehave's own raw, uncaptured Tree narration
// on a mismatch instead of the rich formatted block. Pulled out here once
// so both binaries render mismatches identically and can't drift again.
//
// Every self-test binary that includes this header is expected to have
// already done `using namespace BabyBehave::BDD;` and to include it at
// namespace scope (not inside its own anonymous namespace) - the
// `namespace { ... }` below gives each including TU its own independent,
// internally-linked copy of every function/static here, same as if it had
// been pasted in by hand.

#pragma once

#include <BabyBehave/bdd.hpp>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace BabyBehave::BDD;

namespace {

// Terse overload for scenarios with no single BabyBehaveTest chain to
// capture narration from (e.g. a bare TestContext::Get<T> missing-key
// throw) - just the "did it behave as expected" verdict, no diagnostic
// block possible.
void ReportScenario(const std::string& name, bool passed, int& passCount, int& totalCount) {
    ++totalCount;
    if (passed) {
        ++passCount;
    }
    std::cout << (passed ? "[OK]   " : "[FAIL] ") << name << '\n';
}

// ---------------------------------------------------------------------
// Rich mismatch diagnostics.
//
// BabyBehave's own narration header ("[ FAIL ] name") answers "what
// happened" (the raw scenario outcome); this file's own "[OK]/[FAIL]"
// line answers a different question - "did that match what this
// scenario was written to prove" (asExpected). Reusing the same
// OK/FAIL vocabulary for both makes them look like the same judgment
// when they aren't. ReportScenario() below keeps today's terse one-line
// output when the two agree, and expands into an
// "Expected X / <narration> / TEST Y" block - all three questions
// spelled out separately - only when they disagree (or when
// AlwaysShowDiagnosticsFlag() below forces it every time).
// ---------------------------------------------------------------------

// fd-level capture, not a std::cout.rdbuf() swap: PrintLine() writes via
// std::println() when __cpp_lib_print is available (see bdd.hpp), which
// writes straight to the OS file descriptor and bypasses std::cout's
// stream buffer entirely - a rdbuf() swap would silently capture nothing
// on such toolchains.
std::string CaptureStdoutFd(const std::function<void()>& fn) {
    std::fflush(stdout);
#if defined(_WIN32)
    const int savedFd = _dup(_fileno(stdout));
    FILE* const tmp = std::tmpfile();
    _dup2(_fileno(tmp), _fileno(stdout));
#else
    const int savedFd = dup(fileno(stdout));
    FILE* const tmp = std::tmpfile();
    dup2(fileno(tmp), fileno(stdout));
#endif

    fn();
    std::fflush(stdout);

#if defined(_WIN32)
    _dup2(savedFd, _fileno(stdout));
    _close(savedFd);
#else
    dup2(savedFd, fileno(stdout));
    close(savedFd);
#endif

    std::rewind(tmp);
    std::string result;
    char buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), tmp)) > 0) {
        result.append(buf, n);
    }
    std::fclose(tmp);
    return result;
}

// Explicit runtime override for ColorEnabled() below - unset by default,
// meaning "defer to BABYBEHAVE_COLOR/isatty auto-detection" (see
// ColorEnabled()), which is already "active" for the common case of a
// developer running this binary directly in a real terminal. Only one
// flag exists here, so this stays a plain std::optional<bool> rather than
// a bitset - the latter would just be indirection with nothing to pack.
std::optional<bool>& ColorEnabledOverride() {
    static std::optional<bool> override_;
    return override_;
}

void SetColorEnabled(bool enabled) { ColorEnabledOverride() = enabled; }

// BABYBEHAVE_COLOR=always|never overrides auto-detection; otherwise
// colored only on a real TTY, so output stays clean in files/CI/pipes -
// same pattern as BABYBEHAVE_QUIET/BABYBEHAVE_STYLE. SetColorEnabled()
// above takes precedence over both when set.
bool ColorEnabled() {
    if (ColorEnabledOverride().has_value()) {
        return *ColorEnabledOverride();
    }
    static const bool autoDetected = [] {
        if (const char* env = std::getenv("BABYBEHAVE_COLOR"); env != nullptr) {
            const std::string_view value(env);
            if (value == "always") {
                return true;
            }
            if (value == "never") {
                return false;
            }
        }
#if defined(_WIN32)
        return _isatty(_fileno(stdout)) != 0;
#else
        return isatty(fileno(stdout)) != 0;
#endif
    }();
    return autoDetected;
}

std::string Colorize(const std::string& text, const char* ansiCode) {
    if (!ColorEnabled()) {
        return text;
    }
    return std::string("\033[") + ansiCode + "m" + text + "\033[0m";
}

std::string Red(const std::string& text) { return Colorize(text, "31"); }
std::string Green(const std::string& text) { return Colorize(text, "32"); }

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t pos = text.find('\n', start);
        if (pos == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return lines;
}

std::string ToTitleCaseAscii(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    std::string out = s;
    for (std::size_t i = 1; i < out.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

// One narration line's box-drawing marker, if it has one - "  ├─ WHEN..."
// or "  │  ╰─ WITH...". Lines with neither marker are pure connector
// spacers ("  │") and carry no content, hence std::nullopt.
struct TreeLineParts {
    std::size_t markerPos;
    std::size_t markerLen;
    bool nested; // one level deeper (WITH under GIVEN, AND/OR/BUT under THEN)
    std::string label;
    std::string name;
};

std::optional<TreeLineParts> ParseTreeLine(const std::string& line) {
    const std::string lastMarker(detail::kTreeBranchLast);
    const std::string midMarker(detail::kTreeBranchMid);
    std::size_t markerPos = line.find(lastMarker);
    if (markerPos == std::string::npos) {
        markerPos = line.find(midMarker);
    }
    if (markerPos == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t markerLen = lastMarker.size(); // both markers are the same byte length
    const std::string rest = line.substr(markerPos + markerLen);

    const std::size_t labelEnd = rest.find(' ');
    if (labelEnd == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t nameStart = rest.find_first_not_of(' ', labelEnd);
    return TreeLineParts{ markerPos, markerLen, markerPos > 2, rest.substr(0, labelEnd),
                           nameStart == std::string::npos ? "" : rest.substr(nameStart) };
}

// What ReportScenario() below found out about one scenario.
struct ScenarioDiagnostic {
    std::string scenarioName;
    std::string expectedLabel; // "OK" or "FAIL" - what the scenario was written to prove
    bool testPassed;           // whether the actual outcome matched expectedLabel
    std::string narrationBody; // captured Tree narration, minus BabyBehave's own header line
};

// Pluggable so a caller can swap in its own layout without touching
// ReportScenario() - see SetDiagnosticFormatter() below. Two are provided:
// FlatDiagnosticFormatter() (the default - plain indentation, no
// box-drawing, easiest to read in a scrolling log) and
// TreeDiagnosticFormatter() (keeps BabyBehave's own Tree glyphs, denser and
// more structured for a scenario with many branches).
using DiagnosticFormatter = std::function<std::string(const ScenarioDiagnostic&)>;

std::string DiagnosticHeader(const ScenarioDiagnostic& d, const char* separator) {
    const bool expectsFail = d.expectedLabel == "FAIL";
    const std::string header =
        "Expected " + d.expectedLabel + (expectsFail ? " ✗" : " ✓") + separator + d.scenarioName;
    return expectsFail ? Red(header) : Green(header);
}

// What BabyBehave's own narration header actually said ("[ OK ]"/"[ FAIL ]"),
// as opposed to d.expectedLabel (what the scenario was written to prove).
// The two only diverge when d.testPassed is false - there are exactly two
// possible native outcomes, so "not what we expected" pins down which one
// actually happened without needing to capture it separately.
std::string ActualResultLabel(const ScenarioDiagnostic& d) {
    if (d.testPassed) {
        return d.expectedLabel;
    }
    return d.expectedLabel == "FAIL" ? "OK" : "FAIL";
}

std::string DiagnosticFooter(const ScenarioDiagnostic& d) {
    return "Result " + ActualResultLabel(d) + " - Test " + (d.testPassed ? "OK" : "NOK");
}

std::string FlatDiagnosticFormatter(const ScenarioDiagnostic& d) {
    std::string body;
    for (const auto& line : SplitLines(d.narrationBody)) {
        const auto parts = ParseTreeLine(line);
        if (!parts) {
            continue; // blank connector-only line
        }
        const std::string flatLabel = parts->nested  ? ToTitleCaseAscii(parts->label)
                                       : parts->label == "GIVEN" ? "GIVEN a"
                                                                  : parts->label;
        body += std::string(parts->nested ? 4 : 2, ' ') + flatLabel + " " + parts->name + "\n";
    }

    const std::string footer = "  " + DiagnosticFooter(d);
    return DiagnosticHeader(d, " - ") + "\n" + body + (d.testPassed ? Green(footer) : Red(footer)) + "\n";
}

std::string TreeDiagnosticFormatter(const ScenarioDiagnostic& d) {
    std::vector<std::string> lines;
    for (const auto& line : SplitLines(d.narrationBody)) {
        const auto parts = ParseTreeLine(line);
        if (!parts) {
            continue; // blank connector-only line
        }
        lines.push_back(line.substr(0, parts->markerPos + parts->markerLen) + parts->label + " " + parts->name);
    }

    const std::string lastMarker(detail::kTreeBranchLast);
    if (!lines.empty()) {
        if (const std::size_t pos = lines.back().find(lastMarker); pos != std::string::npos) {
            lines.back().replace(pos, lastMarker.size(), std::string(detail::kTreeBranchMid));
        }
    }

    const std::string footer = "  " + lastMarker + DiagnosticFooter(d); // lastMarker already ends with a space
    std::string out = DiagnosticHeader(d, " : ") + "\n";
    for (const auto& line : lines) {
        out += line + "\n";
    }
    return out + (d.testPassed ? Green(footer) : Red(footer)) + "\n";
}

// Seeded from BABYBEHAVE_STYLE, same pattern (and same env var - reused
// rather than a separate one) as bdd.hpp's own detail::NarrationStyleFlag():
// "tree" picks TreeDiagnosticFormatter, anything else (including unset)
// picks FlatDiagnosticFormatter. The coverage-bbh CMake target's COMMAND
// lines are fixed and invoke these binaries with no arguments, so an
// exported env var - which scripts/coverage.sh's --style= forwards - is
// the only thing that reaches this choice there; --style= on the CLI (see
// ParseCommandLine below) additionally lets a direct invocation override it.
DiagnosticFormatter& ActiveDiagnosticFormatter() {
    static DiagnosticFormatter formatter = [] {
        const char* env = std::getenv("BABYBEHAVE_STYLE");
        return (env != nullptr && std::string_view(env) == "tree") ? DiagnosticFormatter(TreeDiagnosticFormatter)
                                                                     : DiagnosticFormatter(FlatDiagnosticFormatter);
    }();
    return formatter;
}

void SetDiagnosticFormatter(DiagnosticFormatter formatter) { ActiveDiagnosticFormatter() = std::move(formatter); }

// On by default: ReportScenario's rich overload renders the full
// Expected/Result/TEST block for every scenario, passing or not - a
// passing scenario is exactly the "here's proof this behaves as
// documented" case a reader most wants to see, not just a bare "[OK]"
// with nothing to back it up. Set BABYBEHAVE_DIAGNOSTICS=mismatch (or
// --diagnostics=mismatch on the CLI - see ParseCommandLine below) for the
// old terse behavior - a one-line "[OK]"/"[FAIL]" per scenario, full block
// only when actual and expected disagree - for a quieter/shorter CI log.
// Same env-var-reaches-coverage-bbh reasoning as ActiveDiagnosticFormatter()
// above.
bool& AlwaysShowDiagnosticsFlag() {
    static bool always = [] {
        const char* env = std::getenv("BABYBEHAVE_DIAGNOSTICS");
        return !(env != nullptr && std::string_view(env) == "mismatch");
    }();
    return always;
}

void SetAlwaysShowDiagnostics(bool always) { AlwaysShowDiagnosticsFlag() = always; }

// Always Tree, regardless of the ambient BABYBEHAVE_STYLE: ParseTreeLine()
// (and so both FlatDiagnosticFormatter()/TreeDiagnosticFormatter() above)
// only understands Tree's box-drawing glyphs - Plain has no single header
// line to key off of, and Arrow's "->"/"+ " markers wouldn't match either,
// silently producing an empty body. This is independent of whatever style
// the ambient BABYBEHAVE_STYLE selects for scenarios not wired through
// ReportScenario's rich overload (e.g. test_SelfTest.cpp's
// RunNarrationStyleScenario).
NarrationStyle DiagnosticNarrationStyle() { return NarrationStyle::Tree; }

// Overload used by scenarios that run exactly one BabyBehaveTest through
// its normal Execute() path: forces Tree narration on for the duration of
// the call, captures it, and only prints the full Expected/TEST block if
// runScenario() reports a mismatch (or AlwaysShowDiagnosticsFlag() is set).
// Scenarios with a different shape (no BabyBehaveTest at all, a manual
// Execute() call, or ones that manage SetNarrationEnabled/SetNarrationStyle
// themselves to test those very features) keep using the bool overload
// above instead, since forcing narration state around them would fight
// what they're already doing.
void ReportScenario(const std::string& name, const std::string& expectedLabel,
                     const std::function<bool()>& runScenario, int& passCount, int& totalCount) {
    ++totalCount;

    const NarrationStyle savedStyle = detail::NarrationStyleFlag();
    SetNarrationStyle(DiagnosticNarrationStyle());

    bool asExpected = false;
    const std::string captured = CaptureStdoutFd([&] { asExpected = runScenario(); });

    SetNarrationStyle(savedStyle);

    if (asExpected) {
        ++passCount;
    }

    if (asExpected && !AlwaysShowDiagnosticsFlag()) {
        std::cout << Green("[OK]   ") << name << '\n';
        return;
    }

    std::cout << (asExpected ? Green("[OK]   ") : Red("[FAIL] ")) << name << '\n';

    const std::size_t headerEnd = captured.find('\n');
    if (headerEnd == std::string::npos) {
        // Narration produced nothing (e.g. BABYBEHAVE_QUIET set) - fall
        // back to the terse line above; the scenario's own std::cerr
        // dump (see each RunXScenario() below) still carries the detail.
        return;
    }
    const ScenarioDiagnostic diagnostic{ name, expectedLabel, asExpected, captured.substr(headerEnd + 1) };
    std::cout << ActiveDiagnosticFormatter()(diagnostic) << '\n';
}

// Optional startup configuration, an alternative to exporting
// BABYBEHAVE_STYLE/BABYBEHAVE_COLOR/BABYBEHAVE_DIAGNOSTICS before invoking
// this binary:
//   --style=plain|arrow|tree  -> SetNarrationStyle() (BabyBehave's own
//                                ambient style, used as-is by scenarios not
//                                wired through ReportScenario's rich
//                                overload) PLUS SetDiagnosticFormatter():
//                                "tree" picks TreeDiagnosticFormatter,
//                                anything else picks FlatDiagnosticFormatter
//                                - independent of DiagnosticNarrationStyle()
//                                above, which always captures Tree
//                                narration to feed whichever formatter is
//                                active.
//   --color=always|never      -> SetColorEnabled()
//   --diagnostics=always      -> SetAlwaysShowDiagnostics(true) (default):
//                                render the Expected/Result/TEST block for
//                                every scenario, not just mismatches.
//   --diagnostics=mismatch    -> SetAlwaysShowDiagnostics(false): terse
//                                "[OK]"/"[FAIL]" per scenario, full block
//                                only on a mismatch.
// Unrecognized arguments are ignored - these binaries otherwise take none.
void ParseCommandLine(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg.starts_with("--style=")) {
            const std::string style(arg.substr(8));
            SetNarrationStyle(detail::ParseNarrationStyleEnv(style.c_str()));
            SetDiagnosticFormatter(style == "tree" ? TreeDiagnosticFormatter : FlatDiagnosticFormatter);
        } else if (arg == "--color=always") {
            SetColorEnabled(true);
        } else if (arg == "--color=never") {
            SetColorEnabled(false);
        } else if (arg == "--diagnostics=always") {
            SetAlwaysShowDiagnostics(true);
        } else if (arg == "--diagnostics=mismatch") {
            SetAlwaysShowDiagnostics(false);
        }
    }
}

} // namespace
