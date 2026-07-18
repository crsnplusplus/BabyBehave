#ifndef BABYBEHAVE_REPORTERS_HPP
#define BABYBEHAVE_REPORTERS_HPP

#pragma once

// Optional, opt-in CI report serializers for BabyBehave::BDD::TestResult.
//
// This header exists purely to turn already-collected TestResult/StepResult
// data (see bdd.hpp) into two widely-understood, machine-readable text
// formats:
//
//   - JUnit XML  (ToJUnitXml) - understood natively by GitHub Actions test
//     report actions, GitLab CI's "JUnit report" artifact type, Jenkins'
//     JUnit plugin, and most other CI dashboards.
//   - TAP        (ToTap)      - Test Anything Protocol, a simple line-based
//     format consumable by `prove` and many other generic TAP harnesses.
//
// It deliberately lives in its own header, separate from bdd.hpp, so that
// consumers who never call SetCollectFailuresMode(true)/want a report never
// pay for this code (it isn't pulled in unless this header is included) -
// the same reasoning that keeps matchers.hpp standalone. Unlike
// matchers.hpp (which has zero knowledge of bdd.hpp on purpose, so it is
// usable outside BabyBehave entirely), this header fundamentally exists to
// format TestResult/StepResult, so it directly #includes "bdd.hpp" for
// those types - there's no meaningful way to decouple from the very data
// structures it serializes.
//
// IMPORTANT CONSTRAINT: a TestResult only has a complete, meaningful
// StepResult list when the scenario it came from ran with
// SetCollectFailuresMode(true). In BabyBehaveTest's default mode, a failed
// step (or a caught exception) invokes the default
// onConditionNotVerified/onException callback, which calls std::exit()
// before Execute() ever returns - so there is no TestResult to serialize in
// that case, and a scenario that fully passes trivially needs no report
// (nothing failed). Only feed TestResults from scenarios that opted into
// SetCollectFailuresMode(true) (and called Execute()/GetResult() while the
// BabyBehaveTest was still alive - see bdd.hpp's Execute() comment) into
// the functions below; TestResults from default-mode scenarios are either
// unreachable (process exited) or - if a scenario never failed at all -
// simply an empty-but-valid TestResult{testName, true, {}}, which both
// functions below still handle correctly (a passing testsuite/TAP plan with
// zero testcases contributed for that scenario).
//
// Typical use, once you have accumulated TestResults from several
// SetCollectFailuresMode(true) scenarios (see examples/SelfTest.cpp for a
// worked example):
//
//   std::vector<TestResult> results = { result1, result2, result3 };
//   std::cout << BabyBehave::BDD::Reporters::ToJUnitXml(results) << '\n';
//   std::cout << BabyBehave::BDD::Reporters::ToTap(results) << '\n';
//
// Both functions are pure: they take TestResult data in and return a
// formatted std::string, with no file I/O of their own - it's up to the
// caller to print it, write it to a file, or pipe it wherever CI expects
// it.

#include "bdd.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace BabyBehave::BDD::Reporters {

    namespace detail {

        // Escapes the 5 characters XML requires escaped in attribute values
        // and text content: & < > " '. Order matters - '&' must be escaped
        // first, or the ampersands introduced by escaping the other
        // characters would themselves get re-escaped.
        inline std::string EscapeXml(const std::string& text) {
            std::string escaped;
            escaped.reserve(text.size());
            for (const char c : text) {
                switch (c) {
                    case '&':  escaped += "&amp;";  break;
                    case '<':  escaped += "&lt;";   break;
                    case '>':  escaped += "&gt;";   break;
                    case '"':  escaped += "&quot;"; break;
                    case '\'': escaped += "&apos;"; break;
                    default:   escaped += c;        break;
                }
            }
            return escaped;
        }

        // StepResult::location is formatted by bdd.hpp's FormatLocation() as
        // "file:line" (see detail::FormatLocation in bdd.hpp). Splits it back
        // apart on the LAST ':' (not the first) so this doesn't misparse
        // Windows-style "C:\path\to\file.cpp:42" paths, which contain an
        // earlier ':' after the drive letter. Returns {"", ""} when location
        // is empty (no <source_location> support, or a StepResult that
        // predates this field) or has no ':' at all.
        struct SplitLocation {
            std::string file;
            std::string line;
        };

        inline SplitLocation SplitLocationString(const std::string& location) {
            if (location.empty()) {
                return {};
            }
            const auto pos = location.rfind(':');
            if (pos == std::string::npos) {
                return {};
            }
            return SplitLocation{ .file = location.substr(0, pos), .line = location.substr(pos + 1) };
        }

        // "{stepLabel}: {stepName}", the per-testcase/per-TAP-line
        // description shared by both formats below, kept in one place so
        // JUnit and TAP output stay consistent with each other.
        inline std::string StepDescription(const StepResult& step) {
            return step.stepLabel + ": " + step.stepName;
        }

    } // namespace detail

    // Serializes `results` (typically the accumulated TestResults from
    // several SetCollectFailuresMode(true) scenarios - see this header's
    // top comment for why) into a single JUnit XML <testsuite>, with one
    // <testcase> per StepResult across all of them (classname="{testName}",
    // name="{stepLabel}: {stepName}"). A StepResult with passed == false
    // gets a nested <failure message="..."> child carrying its message.
    // When a StepResult::location is available (see bdd.hpp's
    // __cpp_lib_source_location handling), it is split into file/line
    // <testcase> attributes, per common JUnit XML convention.
    inline std::string ToJUnitXml(const std::vector<TestResult>& results, const std::string& suiteName = "BabyBehave") {
        std::size_t testCount = 0;
        std::size_t failureCount = 0;
        for (const auto& result : results) {
            for (const auto& step : result.steps) {
                ++testCount;
                if (!step.passed) {
                    ++failureCount;
                }
            }
        }

        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<testsuite name=\"" << detail::EscapeXml(suiteName) << "\""
            << " tests=\"" << testCount << "\""
            << " failures=\"" << failureCount << "\""
            << " errors=\"0\">\n";

        for (const auto& result : results) {
            for (const auto& step : result.steps) {
                xml << "  <testcase classname=\"" << detail::EscapeXml(result.testName) << "\""
                    << " name=\"" << detail::EscapeXml(detail::StepDescription(step)) << "\"";

                const auto split = detail::SplitLocationString(step.location);
                if (!split.file.empty()) {
                    xml << " file=\"" << detail::EscapeXml(split.file) << "\""
                        << " line=\"" << detail::EscapeXml(split.line) << "\"";
                }

                if (step.passed) {
                    xml << "/>\n";
                } else {
                    xml << ">\n";
                    xml << "    <failure message=\"" << detail::EscapeXml(step.message) << "\">"
                        << detail::EscapeXml(step.message) << "</failure>\n";
                    xml << "  </testcase>\n";
                }
            }
        }

        xml << "</testsuite>\n";
        return xml.str();
    }

    // Single-TestResult convenience overload; equivalent to
    // ToJUnitXml(std::vector<TestResult>{result}, suiteName).
    inline std::string ToJUnitXml(const TestResult& result, const std::string& suiteName = "BabyBehave") {
        return ToJUnitXml(std::vector<TestResult>{ result }, suiteName);
    }

    // Serializes `results` into TAP (Test Anything Protocol) output: a
    // "1..N" plan line (N = total StepResult count across every
    // TestResult), then one "ok"/"not ok" line per StepResult (in the same
    // one-line-per-step granularity as ToJUnitXml(), for consistency
    // between the two formats), each numbered sequentially from 1 and
    // described as "{testName} :: {stepLabel}: {stepName}". A failing step
    // is followed by a "# {message}" TAP diagnostic line carrying its
    // failure message (only emitted when the message is non-empty).
    inline std::string ToTap(const std::vector<TestResult>& results) {
        std::size_t testCount = 0;
        for (const auto& result : results) {
            testCount += result.steps.size();
        }

        std::ostringstream tap;
        tap << "1.." << testCount << "\n";

        std::size_t number = 0;
        for (const auto& result : results) {
            for (const auto& step : result.steps) {
                ++number;
                tap << (step.passed ? "ok " : "not ok ") << number
                    << " - " << result.testName << " :: " << detail::StepDescription(step) << "\n";
                if (!step.passed && !step.message.empty()) {
                    tap << "# " << step.message << "\n";
                }
            }
        }

        return tap.str();
    }

    // Single-TestResult convenience overload; equivalent to
    // ToTap(std::vector<TestResult>{result}).
    inline std::string ToTap(const TestResult& result) {
        return ToTap(std::vector<TestResult>{ result });
    }

} // namespace BabyBehave::BDD::Reporters

#endif // BABYBEHAVE_REPORTERS_HPP
