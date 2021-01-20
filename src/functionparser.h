#pragma once

#include <string>
#include <nlohmann/json_fwd.hpp>

namespace exprtk { // exprtk.hpp
template <typename T> class parser;
template <typename T> class expression;
template <typename T> class symbol_table;
}

/**
 * Since parser<T> is non-copyable we instantiate it
 * with a shared pointer, allowing `ExprFunction`
 * to be directly assigned to `std::function`.
 */
template <typename T = double> class ExprFunction {
    std::shared_ptr<exprtk::parser<T>> parser;
    std::shared_ptr<exprtk::expression<T>> expression;
    std::shared_ptr<exprtk::symbol_table<T>> symbols;
    using VariableVector = std::vector<std::pair<std::string, T *>>;
    using ConstantVector = std::vector<std::pair<std::string, T>>;

  public:
    ExprFunction();
    void set(const std::string &exprstr, const VariableVector &variables = {}, const ConstantVector &constants = {});
    void set(const nlohmann::json &, const VariableVector &variables = {});
    T operator()() const;
};

extern template class ExprFunction<double>;

