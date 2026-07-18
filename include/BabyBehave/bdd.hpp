#ifndef BABYBEHAVE_BDD_HPP
#define BABYBEHAVE_BDD_HPP

#pragma once

#include <functional>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_map>
#include <any>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <iostream>
#include <cstdlib>
#include <version>
#if defined(__cpp_lib_print)
#include <print>
#endif


namespace BabyBehave::BDD {

    namespace detail {
        inline void PrintLine(const std::string& text) {
#if defined(__cpp_lib_print)
            std::println("{}", text);
#else
            std::cout << text << '\n';
#endif
        }

        inline void PrintLine() {
#if defined(__cpp_lib_print)
            std::println();
#else
            std::cout << '\n';
#endif
        }
    } // namespace detail

    class TestContext {
    private:
        std::unordered_map<std::string, std::any> m_objects;

    public:
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = std::move(obj);
        }

        template<typename T>
        [[nodiscard]] T Get(const std::string& key) const {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                const auto errorMsg = "Key not found: " + key;
                std::cerr << errorMsg << std::endl;
                throw std::out_of_range(errorMsg);
            }
            return std::any_cast<T>(it->second);
        }
    };

#if defined(__cpp_lib_move_only_function)
    using StepFunction = std::move_only_function<bool(TestContext&)>;
    using ContextSetupFunction = std::move_only_function<void(TestContext&)>;
#else
    using StepFunction = std::function<bool(TestContext&)>;
    using ContextSetupFunction = std::function<void(TestContext&)>;
#endif

    struct Precondition { StepFunction fn; };
    struct Action { StepFunction fn; };
    struct Postcondition { StepFunction fn; };
    struct And { StepFunction fn; };
    struct Or { StepFunction fn; };
    struct But { StepFunction fn; };

    class BabyBehaveTest {
    public:
        using StepVariant = std::variant<Precondition, Action, Postcondition, And, Or, But>;
        using Step = std::pair<std::string, StepVariant>;


        BabyBehaveTest(const std::string& testName, ContextSetupFunction contextSetupFn)
            : m_testName(testName),
            m_contextSetupFn(std::move(contextSetupFn)) {
            m_onConditionNotVerifiedCallback = [](const std::string& errorMsg) {
                std::cerr << errorMsg << std::endl;
                std::exit(EXIT_FAILURE);
            };
            m_onExceptionCallback = [](const std::string& step, const std::exception& e) {
                std::cerr << "Exception caught in " << step << ":" 
                          << std::string(e.what()) << std::endl;
                std::exit(EXIT_FAILURE);
            };
        }

        ~BabyBehaveTest() {
            Execute();
        }

        BabyBehaveTest(const BabyBehaveTest&) = delete;
        BabyBehaveTest& operator=(const BabyBehaveTest&) = delete;

        void SetOnConditionNotVerifiedCallback(std::function<void(const std::string& errorMsg)> callback) {
            m_onConditionNotVerifiedCallback = callback;
        }

        void SetOnExceptionCallback(std::function<void(const std::string& msg, const std::exception&)> callback) {
            m_onExceptionCallback = callback;
        }

        template<typename StepType>
        BabyBehaveTest& AddStep(const std::string& name, StepFunction stepFunction) {
            StepVariant step = StepType{ std::move(stepFunction) };
            m_steps.push_back({ name, std::move(step) });
            return *this;
        }

        const std::vector<Step>& GetSteps() const {
            return m_steps;
        }

    private:
        void Execute() {
            detail::PrintLine("Given a: " + m_testName);
            try {
                m_contextSetupFn(m_context);
            }
            catch (const std::exception& e) {
                VerifyCondition(false, "Exception caught in Context Setup: " + std::string(e.what()));
            }
            catch (...) {
                VerifyCondition(false, "Exception caught in Context Setup: unknown non-std::exception type thrown");
            }

            for (auto& step : m_steps) {
                std::visit([this, &step](auto&& arg) {
                    executeStep(step.first, arg);
                    }, step.second);
            }

            detail::PrintLine();
        }

        template<typename T>
        void executeStep(const std::string& name, T& step) {
            if constexpr (std::is_same_v<T, Precondition>) {
                detail::PrintLine("    With: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "Precondition failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("Precondition", e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("Precondition", unknownEx);
                }
            } else if constexpr (std::is_same_v<T, Action>) {
                detail::PrintLine("    When: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "Action failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("Action", e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("Action", unknownEx);
                }
            } else if constexpr (std::is_same_v<T, Postcondition>) {
                detail::PrintLine("    Then: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "Postcondition failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("Postcondition", e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("Postcondition", unknownEx);
                }
            } else if constexpr (std::is_same_v<T, And>) {
                detail::PrintLine("    And: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "And condition failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("And condition" , e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("And condition", unknownEx);
                }
            } else if constexpr (std::is_same_v<T, Or>) {
                detail::PrintLine("    Or: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "Or condition failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("Or condition", e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("Or condition", unknownEx);
                }
            } else if constexpr (std::is_same_v<T, But>) {
                detail::PrintLine("    But: " + name);
                try {
                    VerifyCondition(step.fn(m_context), "But condition failed");
                }
                catch (const std::exception& e) {
                    SafeInvokeExceptionCallback("But condition", e);
                }
                catch (...) {
                    const std::runtime_error unknownEx("unknown non-std::exception type thrown");
                    SafeInvokeExceptionCallback("But condition", unknownEx);
                }
            } else {
                static_assert(!sizeof(T), "Unknown step type");
            }
        }


        void VerifyCondition(bool condition, const std::string& errorMsg) const {
            if (!condition) {
                try {
                    m_onConditionNotVerifiedCallback(errorMsg);
                }
                catch (...) {
                    std::cerr << "BabyBehave: onConditionNotVerified callback itself threw an exception; ignoring to avoid std::terminate()" << std::endl;
                }
            }
        }

        void SafeInvokeExceptionCallback(const std::string& step, const std::exception& e) const {
            try {
                m_onExceptionCallback(step, e);
            }
            catch (...) {
                std::cerr << "BabyBehave: onException callback itself threw an exception; ignoring to avoid std::terminate()" << std::endl;
            }
        }

    private:
        std::string m_testName;
        ContextSetupFunction m_contextSetupFn;
        TestContext m_context;
        std::vector<Step> m_steps;

        std::function<void(const std::string& errorMsg)> m_onConditionNotVerifiedCallback;
        std::function<void(const std::string& step, const std::exception&)> m_onExceptionCallback;
    };


    inline BabyBehaveTest GivenAImpl(const std::string& testName,
        ContextSetupFunction contextSetup) {
        return BabyBehaveTest(testName, std::move(contextSetup));
    }

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
} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP
