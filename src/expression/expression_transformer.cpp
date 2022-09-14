//
// Created by JinHai on 2022/9/14.
//

#include "expression_transformer.h"

namespace infinity {

std::vector<std::shared_ptr<BaseExpression>>
SplitExpressionByDelimiter(const std::shared_ptr<BaseExpression> &expression, ConjunctionType delimiter) {
    std::vector<std::shared_ptr<BaseExpression>> result;
    VisitExpression(expression, [&](const std::shared_ptr<BaseExpression> &expr_ptr) {
        if(expr_ptr->type() == ExpressionType::kConjunction) {
            const auto conjunction_expr_ptr = std::static_pointer_cast<ConjunctionExpression>(expr_ptr);
            if(conjunction_expr_ptr->conjunction_type() == delimiter) return VisitControlType::kVisit;
        }
        result.emplace_back(expr_ptr);
        return VisitControlType::kNotVisit;
    });

    return result;
}

void
VisitExpression(const std::shared_ptr<BaseExpression>& expression,
                const std::function<VisitControlType(std::shared_ptr<BaseExpression> &child)>& visitor) {
    std::queue<std::shared_ptr<BaseExpression>> queue;
    queue.push(expression);

    while (!queue.empty()) {
        auto expr = queue.front();
        queue.pop();

        if(visitor(expr) == VisitControlType::kVisit) {
            for(auto& argument: expr->arguments()) {
                queue.push(argument);
            }
        }
    }
}

}