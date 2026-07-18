#ifndef BABYBEHAVE_MATCHERS_HPP
#define BABYBEHAVE_MATCHERS_HPP

#pragma once

// Small, standalone, dependency-free fluent-matcher/assertion helper.
//
// BabyBehave step bodies are plain `bool(TestContext&)` functions: a step
// returns true/false, and on false bdd.hpp only reports a generic label
// ("Precondition failed", "Postcondition failed", ...) with no detail about
// *what* was actually wrong. This header is an OPTIONAL helper consumers can
// call from inside their own step bodies to get a fluent comparison syntax
// plus a descriptive actual-vs-expected message on std::cerr when a check
// fails - without requiring any change to bdd.hpp itself. It has no include
// on bdd.hpp and no knowledge of TestContext/BabyBehaveTest; it is just as
// usable outside BabyBehave entirely.
//
// Typical use inside a step:
//
//   bool AlarmWillRing(TestContext& context) {
//       auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
//       return BabyBehave::Matchers::Expect(alarmClock->GetHour()).ToEqual(7);
//   }
//
// On failure this prints something like:
//   Expect(...) failed: expected 6 to equal 7
// to std::cerr, then returns false, which bdd.hpp's existing step-failure
// machinery picks up exactly as it would any other `return false;`.

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

        // Always-false trap for static_assert inside `if constexpr` chains
        // (a bare `static_assert(false, ...)` would fire even on branches
        // that are never instantiated; this defers evaluation to
        // instantiation time, the same trick used elsewhere for this
        // problem).
        template<typename>
        inline constexpr bool always_false_v = false;

        // Detects whether `os << value` is well-formed for T, so PrintValue
        // below can fall back to a placeholder instead of failing to
        // compile for non-streamable types (e.g. a plain struct with no
        // operator<<).
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

    // Fluent wrapper returned by Expect(value). Every ToXxx() method returns
    // bool (so it can be used directly as a step's `return ...;`) and, on
    // failure, writes a descriptive "expected ... to ... " message to
    // std::cerr showing the actual value (and the expected one, where
    // applicable) before returning false. Values that aren't
    // stream-insertable print as "(non-printable value)" instead of failing
    // to compile.
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

    // Entry point. Deduces and decay-copies the value into an Expectation<T>
    // wrapper; chain one of the ToXxx() methods above on the result, e.g.
    //   Expect(alarmClock->GetHour()).ToEqual(7)
    template<typename T>
    Expectation<std::decay_t<T>> Expect(T&& value) {
        return Expectation<std::decay_t<T>>(std::forward<T>(value));
    }

} // namespace BabyBehave::Matchers

#endif // BABYBEHAVE_MATCHERS_HPP
