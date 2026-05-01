#ifndef GRAPHDB_COMMON_VALUE_HPP
#define GRAPHDB_COMMON_VALUE_HPP
#include <variant>
#include <memory>

namespace graph {
using Int = int64_t;
using Double = double;
using String = std::string;
using Bool = bool;

using Value = std::variant<Int, Double, String, Bool>;
}  // namespace graph

namespace storage {
using NodeId = size_t;
using EdgeId = size_t;

using Value = std::variant<int64_t, double, std::string, bool>;
using Properties = std::unordered_map<std::string, Value>;

struct Edge {
  EdgeId id;
  bool alive;

  NodeId src;
  NodeId dst;
  std::string type;
  Properties properties;
};

struct Node {
  NodeId id;
  bool alive;

  std::vector<std::string> labels;
  Properties properties;
};

}  // namespace storage

namespace graph::exec {
using storage::Node;
using storage::Edge;
struct PhysicalOp;
struct RowCursor;
struct Row;
struct ExecContext;

using RowSlot = std::variant<Node*, Edge*, Value>;
using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;
using RowCursorPtr = std::unique_ptr<RowCursor>;

struct LabelIndexSeekOp;
struct NodeScanOp;
template <bool edge_outgoing>
struct ExpandOp;
using ExpandOutgoingOp = ExpandOp<true>;
using ExpandIngoingOp = ExpandOp<false>;

struct NestedLoopJoinOp;
struct KeyHashJoinOp;
struct PhysicalSetOp;
struct PhysicalCreateOp;
struct PhysicalDeleteOp;
} // namespace graph::exec

namespace graph::logical {
struct LogicalOp;
using LogicalOpPtr = std::unique_ptr<LogicalOp>;

enum class LogicalOpType {
  Scan,
  Expand,
  Filter,
  Project,
  Limit,
  Sort,
  Join,
  Set,
  Create,
  Delete,
  Plan
};
}

#endif //GRAPHDB_COMMON_VALUE_HPP