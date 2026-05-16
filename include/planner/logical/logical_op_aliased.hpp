#pragma once

#include "common/common_value.hpp"
#include "logical_op_zero_child.hpp"

namespace graph::logical {
struct AliasedLogicalOp : public LogicalOpZeroChild {
  AliasedLogicalOp(std::string dst_alias) : dst_alias(std::move(dst_alias)) {
  }

public:
  std::string dst_alias;
};
}
