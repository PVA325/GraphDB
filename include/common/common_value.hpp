#ifndef GRAPHDB_COMMON_VALUE_HPP
#define GRAPHDB_COMMON_VALUE_HPP
#include <variant>
#include <tuple>
#include <memory>
#include <vector>
#include "unordered_map"

namespace ast {
// Edge direction.
enum class EdgeDirection {
  Left,
  Right,
  Undirected
};

struct Expr;
struct ReturnItem;
struct OrderItem;
using ExprPtr = std::unique_ptr<Expr>;
}

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
struct GraphDB;
}  // namespace storage

namespace graph::exec {
using storage::Node;
using storage::Edge;
struct PhysicalOp;
struct RowCursor;
struct Row;
struct ExecContext;

struct RowSlot {
  RowSlot(const std::variant<Node*, Edge*, Value>& val): value(val) {}
  RowSlot(const std::variant<Node*, Edge*, Value>& val,
    String out_name, String source_alias_name, String property_name):
    value(val), out_name(std::move(out_name)), source_alias_name(std::move(source_alias_name)),
    property_name(std::move(property_name)) {}

  RowSlot(const RowSlot&) = default;
  RowSlot& operator=(const RowSlot&) = default;
  bool operator==(const RowSlot& slot) const = default;
  bool operator!=(const RowSlot& slot) const = default;

  std::variant<Node*, Edge*, Value> value;
  String out_name;
  String source_alias_name;
  String property_name;

};
using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;
using RowCursorPtr = std::unique_ptr<RowCursor>;

struct LabelIndexSeekOp;
struct NodeScanOp;
template <bool edge_outgoing>
struct ExpandOp;
using ExpandOutgoingOp = ExpandOp<true>;
using ExpandIngoingOp = ExpandOp<false>;

struct NestedLoopJoinOp;
struct HashJoinOp;
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

namespace graph::optimizer {
struct CostModel;
// Cost estimates structure

struct CostEstimate {
  double row_count{0.0}; // count of rows that operator gives us
  double cpu_cost{0.0}; // nominal cost of cpu
  double io_cost{0.0};
  // nominal cost if read/write(or other heavy operations with ram, not cache) operations(for example for index scan)
  double startup_cost{0.0}; // cost to give first row

  [[nodiscard]] double total() const { return startup_cost + cpu_cost + 5.0 * io_cost; } // FOR TEST ONLY, REWRITE

  static double kCostEstimateInf;
  static CostEstimate GetMaxCostEstimate() {
    return {kCostEstimateInf, kCostEstimateInf, kCostEstimateInf, kCostEstimateInf};
  }
};
} // namespace graph::optimizer

namespace graph::logical {
using BuildPhysicalType = std::pair<exec::PhysicalOpPtr, optimizer::CostEstimate>;
}  // namespace graph::logical

#endif //GRAPHDB_COMMON_VALUE_HPP