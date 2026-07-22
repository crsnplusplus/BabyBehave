#ifndef BABYBEHAVE_REPORTERS_HPP
#define BABYBEHAVE_REPORTERS_HPP

#pragma once

// Optional CI report serializers (JUnit XML / TAP) for TestResult data.
// Separate header so consumers never paying for reports don't pull it in.
//
// IMPORTANT: TestResult has complete StepResult list ONLY when the scenario
// ran with SetCollectFailuresMode(true). Default mode calls std::exit() on
// failure, so no TestResult exists to serialize. Only pass TestResults from
// SetCollectFailuresMode(true) scenarios to functions below.
//
// Usage: std::vector<TestResult> results = {result1, result2};
//        std::cout << BabyBehave::BDD::Reporters::ToJUnitXml(results);
//        std::cout << BabyBehave::BDD::Reporters::ToTap(results);
// Both are pure functions (no file I/O; caller prints or pipes the output).

#include "bdd.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace BabyBehave::BDD::Reporters {

    namespace detail {

        // Escapes XML metacharacters (& < > " '). & must be first to avoid
        // double-escaping other replacements.
        inline std::string EscapeXml(const std::string& text) {
            std::string escaped;
            escaped.reserve(text.size());
            for (const char chr : text) {
                switch (chr) {
                    case '&':  escaped += "&amp;";  break;
                    case '<':  escaped += "&lt;";   break;
                    case '>':  escaped += "&gt;";   break;
                    case '"':  escaped += "&quot;"; break;
                    case '\'': escaped += "&apos;"; break;
                    default:   escaped += chr;      break;
                }
            }
            return escaped;
        }

        // StepResult::location is "file:line" (from bdd.hpp's FormatLocation).
        // Split on LAST ':' to handle Windows paths like "C:\path\file.cpp:42".
        // Returns {"", ""} when location is empty or has no ':'.
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

        // "{stepLabel}: {stepName}" — shared description for JUnit/TAP
        // consistency.
        inline std::string StepDescription(const StepResult& step) {
            return step.stepLabel + ": " + step.stepName;
        }

    } // namespace detail

    // Serializes TestResults to JUnit XML: <testsuite> with <testcase> per
    // StepResult (classname="{testName}", name="{stepLabel}: {stepName}").
    // Failed steps get <failure> child with message. Location (if available)
    // split into file/line <testcase> attributes per JUnit convention.
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

    // Convenience overload for single TestResult.
    inline std::string ToJUnitXml(const TestResult& result, const std::string& suiteName = "BabyBehave") {
        return ToJUnitXml(std::vector<TestResult>{ result }, suiteName);
    }

    // Serializes TestResults to TAP: "1..N" plan, then "ok"/"not ok" per
    // StepResult numbered from 1, described as "{testName} :: {stepLabel}:
    // {stepName}". Failing steps followed by "# {message}" diagnostic line.
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

    // Convenience overload for single TestResult.
    inline std::string ToTap(const TestResult& result) {
        return ToTap(std::vector<TestResult>{ result });
    }

} // namespace BabyBehave::BDD::Reporters

#endif // BABYBEHAVE_REPORTERS_HPP
