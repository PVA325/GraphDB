#ifndef GRAPHDB_LABEL_INDEX_SEEK_HPP
#define GRAPHDB_LABEL_INDEX_SEEK_HPP

#include "physical_op_no_child.hpp"

namespace graph::exec {
struct LabelIndexSeekOp : public PhysicalOpNoChild {
  /// used for node filtering(for more optimized by label we dont need to do all node scan)
  String out_alias;
  String label;

  LabelIndexSeekOp(String alias, String label);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~LabelIndexSeekOp() override = default;
};
}

#endif //GRAPHDB_LABEL_INDEX_SEEK_HPP