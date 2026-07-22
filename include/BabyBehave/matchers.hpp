#ifndef BABYBEHAVE_MATCHERS_HPP
#define BABYBEHAVE_MATCHERS_HPP

#pragma once

// Optional fluent-matcher helper for step assertions. Step bodies return
// bool; on false, bdd.hpp only reports a generic label ("Precondition failed",
// etc). This header adds descriptive actual-vs-expected messages to std::cerr.
// Standalone, no dependency on bdd.hpp/TestContext. Example:
//   bool AlarmWillRing(TestContext& context) {
//       auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
//       return BabyBehave::Matchers::Expect(alarmClock->GetHour()).ToEqual(7);
//   }
// On failure: prints "Expect(...) failed: expected 6 to equal 7" to std::cerr,
// then returns false.

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace BabyBehave::Matchers {

    namespace detail {

        // Always-false trap for static_assert in `if constexpr` chains
        // (defers evaluation to instantiation time, avoiding false positives).
        template<typename>
        inline constexpr bool always_false_v = false;

        // Detects whether `os << value` is well-formed for T;
        // PrintValue falls back to placeholder for non-streamable types.
        template<typename T, typename = void>
        struct is_streamable : std::false_type {};

        template<typename T>
        struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
            : std::true_type {};

        template<typename T>
        inline constexpr bool is_streamable_v = is_streamable<T>::value;

        template<typename T>
        void PrintValue(std::ostream& os, const T& value) {
            if constexpr (is_streamable_v<T>) {
                os << value;
            } else {
                os << "(non-printable value)";
            }
        }

        // Detects container-like types usable with std::begin/std::end
        // (vector, set, array, string, ...), for ToContain().
        template<typename T, typename = void>
        struct has_begin_end : std::false_type {};

        template<typename T>
        struct has_begin_end<T, std::void_t<decltype(std::begin(std::declval<const T&>())),
                                             decltype(std::end(std::declval<const T&>()))>>
            : std::true_type {};

        template<typename T>
        inline constexpr bool has_begin_end_v = has_begin_end<T>::value;

        // Detects types convertible to std::string_view (std::string,
        // std::string_view, const char*, ...), so ToContain() can do a
        // substring search instead of an element-wise one when both sides
        // look like strings.
        template<typename T, typename = void>
        struct is_string_like : std::false_type {};

        template<typename T>
        struct is_string_like<T, std::void_t<decltype(std::string_view(std::declval<const T&>()))>>
            : std::true_type {};

        template<typename T>
        inline constexpr bool is_string_like_v = is_string_like<T>::value;

        // Detects types comparable to nullptr (raw pointers, shared_ptr,
        // unique_ptr, ...), for ToBeNull()/ToNotBeNull().
        template<typename T, typename = void>
        struct is_null_comparable : std::false_type {};

        template<typename T>
        struct is_null_comparable<T, std::void_t<decltype(std::declval<const T&>() == nullptr)>>
            : std::true_type {};

        template<typename T>
        inline constexpr bool is_null_comparable_v = is_null_comparable<T>::value;

        // Detects optional-like types (std::optional and friends) via a
        // `.has_value()` member, for ToBeNull()/ToNotBeNull() on types that
        // aren't nullptr-comparable.
        template<typename T, typename = void>
        struct has_has_value_member : std::false_type {};

        template<typename T>
        struct has_has_value_member<T, std::void_t<decltype(std::declval<const T&>().has_value())>>
            : std::true_type {};

        template<typename T>
        inline constexpr bool has_has_value_member_v = has_has_value_member<T>::value;

    } // namespace detail

    // Fluent wrapper returned by Expect(value). ToXxx() methods return bool
    // (usable as step return) and write descriptive "expected ... to ..."
    // messages to std::cerr on failure. Non-streamable values print as
    // "(non-printable value)".
    template<typename T>
    class Expectation {
    public:
        explicit Expectation(T value) : m_value(std::move(value)) {}

        template<typename U>
        [[nodiscard]] bool ToEqual(const U& expected) const {
            const bool ok = (m_value == expected);
            if (!ok) {
                Fail("to equal", expected);
            }
            return ok;
        }

        template<typename U>
        [[nodiscard]] bool ToNotEqual(const U& expected) const {
            const bool ok = !(m_value == expected);
            if (!ok) {
                Fail("to not equal", expected);
            }
            return ok;
        }

        [[nodiscard]] bool ToBeTrue() const {
            if constexpr (std::is_convertible_v<T, bool>) {
                const bool ok = static_cast<bool>(m_value);
                if (!ok) {
                    Fail("to be true");
                }
                return ok;
            } else {
                static_assert(detail::always_false_v<T>, "ToBeTrue() requires a bool-convertible type");
                return false;
            }
        }

        [[nodiscard]] bool ToBeFalse() const {
            if constexpr (std::is_convertible_v<T, bool>) {
                const bool ok = !static_cast<bool>(m_value);
                if (!ok) {
                    Fail("to be false");
                }
                return ok;
            } else {
                static_assert(detail::always_false_v<T>, "ToBeFalse() requires a bool-convertible type");
                return false;
            }
        }

        template<typename U>
        [[nodiscard]] bool ToBeGreaterThan(const U& expected) const {
            const bool ok = (m_value > expected);
            if (!ok) {
                Fail("to be greater than", expected);
            }
            return ok;
        }

        template<typename U>
        [[nodiscard]] bool ToBeGreaterOrEqualTo(const U& expected) const {
            const bool ok = (m_value >= expected);
            if (!ok) {
                Fail("to be greater than or equal to", expected);
            }
            return ok;
        }

        template<typename U>
        [[nodiscard]] bool ToBeLessThan(const U& expected) const {
            const bool ok = (m_value < expected);
            if (!ok) {
                Fail("to be less than", expected);
            }
            return ok;
        }

        template<typename U>
        [[nodiscard]] bool ToBeLessOrEqualTo(const U& expected) const {
            const bool ok = (m_value <= expected);
            if (!ok) {
                Fail("to be less than or equal to", expected);
            }
            return ok;
        }

        // For string-like T/U (std::string, std::string_view, const char*):
        // substring search. Otherwise, for any container usable with
        // std::begin/std::end (vector, set, array, ...): element-wise
        // search via std::find.
        template<typename U>
        [[nodiscard]] bool ToContain(const U& item) const {
            if constexpr (detail::is_string_like_v<T> && detail::is_string_like_v<U>) {
                const std::string_view haystack(m_value);
                const std::string_view needle(item);
                const bool ok = haystack.find(needle) != std::string_view::npos;
                if (!ok) {
                    Fail("to contain", item);
                }
                return ok;
            } else if constexpr (detail::has_begin_end_v<T>) {
                const bool ok = std::find(std::begin(m_value), std::end(m_value), item) != std::end(m_value);
                if (!ok) {
                    Fail("to contain", item);
                }
                return ok;
            } else {
                static_assert(detail::always_false_v<T>,
                    "ToContain() requires a container (begin/end) or string-like type");
                return false;
            }
        }

        // Works for raw pointers, smart pointers (unique_ptr/shared_ptr,
        // anything comparable to nullptr), and optional-like types (has_value()).
        [[nodiscard]] bool ToBeNull() const {
            if constexpr (detail::is_null_comparable_v<T>) {
                const bool ok = (m_value == nullptr);
                if (!ok) {
                    Fail("to be null");
                }
                return ok;
            } else if constexpr (detail::has_has_value_member_v<T>) {
                const bool ok = !m_value.has_value();
                if (!ok) {
                    Fail("to be null (empty)");
                }
                return ok;
            } else {
                static_assert(detail::always_false_v<T>,
                    "ToBeNull() requires a pointer-like or optional-like type");
                return false;
            }
        }

        [[nodiscard]] bool ToNotBeNull() const {
            if constexpr (detail::is_null_comparable_v<T>) {
                const bool ok = (m_value != nullptr);
                if (!ok) {
                    Fail("to not be null");
                }
                return ok;
            } else if constexpr (detail::has_has_value_member_v<T>) {
                const bool ok = m_value.has_value();
                if (!ok) {
                    Fail("to not be null (empty)");
                }
                return ok;
            } else {
                static_assert(detail::always_false_v<T>,
                    "ToNotBeNull() requires a pointer-like or optional-like type");
                return false;
            }
        }

    private:
        template<typename U>
        void Fail(const std::string& verbPhrase, const U& expected) const {
            std::ostringstream oss;
            oss << "Expect(...) failed: expected ";
            detail::PrintValue(oss, m_value);
            oss << " " << verbPhrase << " ";
            detail::PrintValue(oss, expected);
            std::cerr << oss.str() << "\n";
        }

        void Fail(const std::string& verbPhrase) const {
            std::ostringstream oss;
            oss << "Expect(...) failed: expected ";
            detail::PrintValue(oss, m_value);
            oss << " " << verbPhrase;
            std::cerr << oss.str() << "\n";
        }

        T m_value;
    };

    // Entry point. Deduces and wraps value into Expectation<T>;
    // chain ToXxx() method (e.g., Expect(val).ToEqual(7)).
    template<typename T>
    Expectation<std::decay_t<T>> Expect(T&& value) {
        return Expectation<std::decay_t<T>>(std::forward<T>(value));
    }

} // namespace BabyBehave::Matchers

#endif // BABYBEHAVE_MATCHERS_HPP
