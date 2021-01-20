#include <doctest/doctest.h>
#include "functionparser.h"
#include <exprtk.hpp> // https://github.com/ArashPartow/exprtk
#include <nlohmann/json.hpp>

template <typename T> ExprFunction<T>::ExprFunction() {
    parser = std::make_shared<exprtk::parser<T>>();
    symbols = std::make_shared<exprtk::symbol_table<T>>();
    expression = std::make_shared<exprtk::expression<T>>();
}

/**
 * @brief Set expression string, variables, and constants.
 * @param exprstr Expression
 * @param variables Vector of pairs of variables (name, pointer to value)
 * @param constants Vector of pairs of constants (name, value)
 */
template <typename T>
void ExprFunction<T>::set(const std::string &exprstr, const VariableVector &variables,
                          const ConstantVector &constants) {
    symbols->clear();
    for (auto [name, value_ptr] : variables) {
        symbols->add_variable(name, *value_ptr);
    }
    for (auto [name, value] : constants) {
        symbols->add_constant(name, value);
    }
    symbols->add_constants();
    expression->register_symbol_table(*symbols);
    if (!parser->compile(exprstr, *expression)) {
        throw std::runtime_error("error parsing function/expression");
    }
}

template <typename T> void ExprFunction<T>::set(const nlohmann::json &j, const VariableVector &variables) {
    ConstantVector constants; // temporary storage for user-defined constants
    if (auto it = j.find("constants"); it != j.end()) {
        if (it->is_object()) {
            constants.reserve(it->size());
            for (auto [name, value] : it->items()) {
                constants.push_back({name, value});
            }
        } else {
            throw std::runtime_error("`constants` must be an object");
        }
    }
    set(j.at("function").get<std::string>(), variables, constants);
}

template<typename T>
T ExprFunction<T>::operator()() const {
    return expression->value();
}

template class ExprFunction<double>;

TEST_CASE("[Faunus] ExprFunction") {
    double x = 0, y = 0;
    ExprFunction<double> expr;
    nlohmann::json j = R"({ "function": "x*x+kappa", "constants": {"kappa": 0.4, "f": 2} })"_json;
    expr.set(j, {{"x", &x}, {"y", &y}});
    std::function<double()> f = expr;
    x = 4;
    CHECK(f() == doctest::Approx(4 * 4 + 0.4));
}
