#ifndef BABYBEHAVE_BDD_HPP
#define BABYBEHAVE_BDD_HPP

#include <functional>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_map>
#include <any>
#include <string>
#include <stdexcept>
#include <concepts>
#include <iostream>


namespace BabyBehave::BDD {

    class TestContext {
    private:
        std::unordered_map<std::string, std::any> m_objects;

    public:
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = obj;
        }

        template<typename T>
        T Get(const std::string& key) {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                auto errorMsg = "Key not found: " + key;
                std::cout << errorMsg << std::endl;
                throw std::runtime_error(errorMsg);
            }
            return std::any_cast<T>(it->second);
        }
    };

    struct Precondition {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    struct Action {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    struct Postcondition {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    struct And {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    struct Or {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    struct But {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    template<typename T>
    concept IsPrecondition = std::same_as<T, Precondition>;

    template<typename T>
    concept IsAction = std::same_as<T, Action>;

    template<typename T>
    concept IsPostcondition = std::same_as<T, Postcondition>;

    template<typename T>
    concept IsAnd = std::same_as<T, And>;

    template<typename T>
    concept IsOr = std::same_as<T, Or>;

    template<typename T>
    concept IsBut = std::same_as<T, But>;


    class BabyBehaveTest {
    public:
        using StepVariant = std::variant<Precondition, Action, Postcondition, And, Or, But>;
        using Step = std::pair<std::string, StepVariant>;


        BabyBehaveTest(const std::string& testName, std::function<void(TestContext&)> contextSetupFn)
            : m_testName(testName),
            m_contextSetupFn(contextSetupFn) {
            m_onConditionNotVerifiedCallback = [](const std::string& errorMsg) {
                std::cerr << errorMsg << std::endl;
                std::exit(EXIT_FAILURE);
                };
            m_onExceptionCallback = [](const std::string& errorMsg) {
                std::cerr << errorMsg << std::endl;
                std::exit(EXIT_FAILURE);
                };
        }

        ~BabyBehaveTest() {
            Execute();
        }

        void SetOnConditionNotVerifiedCallback(std::function<void(const std::string& errorMsg)> callback) {
            m_onConditionNotVerifiedCallback = callback;
        }

        void SetOnExceptionCallback(std::function<void(const std::string& errorMsg)> callback) {
            m_onExceptionCallback = callback;
        }

        BabyBehaveTest& WithImpl(const std::string& name, Precondition precondition) {
            m_steps.push_back({ name, precondition });
            return *this;
        }

        BabyBehaveTest& WhenImpl(const std::string& name, Action action) {
            m_steps.push_back({ name, action });
            return *this;
        }

        BabyBehaveTest& ThenImpl(const std::string& name, Postcondition postcondition) {
            m_steps.push_back({ name, postcondition });
            return *this;
        }

        BabyBehaveTest& AndImpl(const std::string& name, And andAction) {
            m_steps.push_back({ name, andAction });
            return *this;
        }

        BabyBehaveTest& OrImpl(const std::string& name, Or orAction) {
            m_steps.push_back({ name, orAction });
            return *this;
        }

        BabyBehaveTest& ButImpl(const std::string& name, But butAction) {
            m_steps.push_back({ name, butAction });
            return *this;
        }

    private:
        void Execute() {
            std::cout << "Given a: " << m_testName << std::endl;
            try {
                m_contextSetupFn(m_context);
            }
            catch (const std::exception& e) {
                VerifyCondition(false, "Exception caught in Context Setup: " + std::string(e.what()));
            }

            for (const auto& step : m_steps) {
                std::visit([this, &step](auto&& arg) {
                    executeStep(step.first, arg);
                    }, step.second);
            }

            std::cout << std::endl;
        }

        template<IsPrecondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    With: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Precondition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in Precondition: " + std::string(e.what()));
            }
        }

        template<IsAction T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    When: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Action failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in Action: " + std::string(e.what()));
            }
        }

        template<IsPostcondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    Then: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Precondition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in Precondition: " + std::string(e.what()));
            }
        }

        template<IsAnd T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    And: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "And condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in And condition: " + std::string(e.what()));
            }
        }

        template<IsOr T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    Or: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Or condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in Or condition: " + std::string(e.what()));
            }
        }

        template<IsBut T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    But: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "But condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in But condition: " + std::string(e.what()));
            }
        }


        void VerifyCondition(bool condition, const std::string& errorMsg) {
            if (!condition) {
                m_onConditionNotVerifiedCallback(errorMsg);
            }
        }

    private:
        std::string m_testName;
        std::function<void(TestContext&)> m_contextSetupFn;
        TestContext m_context;
        std::vector<Step> m_steps;

        std::function<void(const std::string& errorMsg)> m_onConditionNotVerifiedCallback;
        std::function<void(const std::string& errorMsg)> m_onExceptionCallback;
    };


    inline BabyBehaveTest GivenAImpl(const std::string& testName,
        std::function<void(TestContext&)> contextSetup) {
        return BabyBehaveTest(testName, contextSetup);
    }


#define Given(func) GivenAImpl(#func, {func})
#define GivenA(func) GivenAImpl(#func, {func})
#define With(func)  WithImpl(#func, {func})
#define WithI(func) WithImpl(#func, {func})
#define When(func)  WhenImpl(#func, {func})
#define WhenI(func) WhenImpl(#func, {func})
#define Then(func)  ThenImpl(#func, {func})
#define ThenI(func) ThenImpl(#func, {func})
#define And(func)   AndImpl(#func, {func})
#define AndI(func)  AndImpl(#func, {func})
#define But(func)   ButImpl(#func, {func})
#define ButI(func)  ButImpl(#func, {func})
#define Or(func)    OrImpl(#func, {func})
#define OrI(func)   OrImpl(#func, {func})
} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP
