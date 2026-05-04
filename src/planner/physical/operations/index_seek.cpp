#include "planner/query_planner.hpp"

namespace graph::exec {

LabelIndexSeekOp::LabelIndexSeekOp(
  String alias, String label) :
  out_alias(std::move(alias)),
  label(std::move(label)) {
}

RowCursorPtr LabelIndexSeekOp::open(ExecContext& ctx) {
  return std::make_unique<NodeScanCursor>(
    std::move(ctx.db->nodes_with_label(label)),
    out_alias
  );
}

}