#pragma once

#include "physical_op_no_child.hpp"

namespace graph::exec {
struct LabelIndexSeekOp : public PhysicalOpNoChild {
  /// used for node filtering(for more optimized by label we dont need to do all node scan)
  LabelIndexSeekOp(String alias, String label);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~LabelIndexSeekOp() override = default;

public:
  String out_alias;
  String label;
};
}
