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
        using Function = std::function<void(TestContext&)>;
        Function fn;
    };

    struct Postcondition {
        using Function = std::function<bool(TestContext&)>;
        Function fn;
    };

    template<typename T>
    concept IsPrecondition = std::same_as<T, Precondition>;

    template<typename T>
    concept IsAction = std::same_as<T, Action>;

    template<typename T>
    concept IsPostcondition = std::same_as<T, Postcondition>;


    class BabyBehaveTest {
    public:
        using StepVariant = std::variant<Precondition, Action, Postcondition>;
        using Step = std::pair<std::string, StepVariant>;


        BabyBehaveTest(const std::string& testName, std::function<void(TestContext&)> contextSetupFn)
            : m_testName(testName),
            m_contextSetupFn(contextSetupFn) {

        }

        ~BabyBehaveTest() {
            Execute();
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

    private:
        void Execute() {
            std::cout << "Given a: " << m_testName << std::endl;
            m_contextSetupFn(m_context);

            for (const auto& step : m_steps) {
                std::visit([this, &step](auto&& arg) {
                    executeStep(step.first, arg);
                    }, step.second);
            }

            std::cout  << std::endl;
        }

        template<IsPrecondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    With: " << name << std::endl;
            VerifyCondition(step.fn(m_context), "Precondition failed");
        }

        template<IsAction T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    When: " << name << std::endl;
            step.fn(m_context);
        }

        template<IsPostcondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    Then: " << name << std::endl;
            VerifyCondition(step.fn(m_context), "Postcondition failed");
        }

        void VerifyCondition(bool condition, const std::string& errorMsg) {
            if (!condition) {
                std::cerr << errorMsg << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }

    private:
        std::string m_testName;
        TestContext m_context;
        std::vector<Step> m_steps;
        std::function<void(TestContext&)> m_contextSetupFn;
    };


    inline BabyBehaveTest GivenAImpl(const std::string& testName, std::function<void(TestContext&)> contextSetup) {
        return BabyBehaveTest(testName, contextSetup);
    }

#define GivenA(func) GivenAImpl(#func, {func})
#define With(func) WithImpl(#func, {func})
#define When(func) WhenImpl(#func, {func})
#define Then(func) ThenImpl(#func, {func})

} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP
