#ifndef GRAPHDB_PHYSICAL_OP_HPP
#define GRAPHDB_PHYSICAL_OP_HPP

#include "common/common_value.hpp"

namespace graph::exec {
struct PhysicalOp {
  virtual ~PhysicalOp() = default;

  /// open operator, return RowCursor
  /// tx may be required for storage access
  virtual RowCursorPtr open(ExecContext& ctx) = 0;

  [[nodiscard]] virtual String DebugString() const = 0;
  [[nodiscard]] virtual String SubtreeDebugString() const = 0;
};
}

#endif //GRAPHDB_PHYSICAL_OP_HPP
