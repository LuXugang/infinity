//
// Created by JinHai on 2022/8/9.
//

#pragma once

#include <utility>

#include "planner/logical_node.h"
#include "expression/base_expression.h"

namespace infinity {

class LogicalFilter : public LogicalNode {
public:
    LogicalFilter(std::shared_ptr<BaseExpression> expression)
        : LogicalNode(LogicalNodeType::kFilter), expression_(std::move(expression)) {}

    std::string ToString(uint64_t space) final;

private:
    std::shared_ptr<BaseExpression> expression_;
};

}
