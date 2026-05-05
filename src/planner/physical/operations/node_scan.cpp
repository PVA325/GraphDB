#include "planner/query_planner.hpp"

namespace graph::exec {

RowCursorPtr NodeScanOp::open(ExecContext& ctx) {
  return std::make_unique<NodeScanCursor>(
    std::move(ctx.db->all_nodes()),
    out_alias
  );
}

}