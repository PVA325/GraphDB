#include "planner/query_planner.hpp"

namespace graph::exec {

NodePropertyFilterOp::NodePropertyFilterOp(PhysicalOpPtr child, String alias, std::vector<String> labels,
                                           std::vector<std::pair<String, Value>> properties) :
  PhysicalOpUnaryChild(std::move(child)),
  alias(std::move(alias)),
  labels(std::move(labels)),
  properties(std::move(properties)) {
}

RowCursorPtr NodePropertyFilterOp::open(ExecContext& ctx) {
  return std::make_unique<NodePropertyFilterCursor>(
    std::move(child->open(ctx)),
    alias,
    labels,
    properties
  );
}

}