#include <format>
#include "graph.hpp"

namespace {
  using String = std::string;
  String IndentBlock(const String& s, const String& indent = "  ") {
    String out;
    size_t pos = 0;

    while (pos < s.size()) {
      size_t end = s.find('\n', pos);
      String line = (end == String::npos) ? s.substr(pos) : s.substr(pos, end - pos);

      if (!line.empty()) {
        out += indent;
        out += line;
      }

      if (end == String::npos) {
        break;
      }
      out += '\n';
      pos = end + 1;
    }

    return out;
  }

  template <class Range, class F>
  String Join(const Range& r, std::string_view sep, F&& f) {
    String out;
    bool first = true;
    for (const auto& x : r) {
      if (!first) {
        out += sep;
      }
      first = false;
      out += f(x);
    }
    return out;
  }

  std::string ValueDebugString(const graph::Value& v) {
    return std::visit([](const auto& x) -> std::string {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, std::string>) {
        return std::format("\"{}\"", x);
      } else if constexpr (std::is_same_v<T, bool>) {
        return x ? "true" : "false";
      } else if constexpr (std::is_arithmetic_v<T>) {
        return std::format("{}", x);
      } else if constexpr (std::is_same_v<T, std::monostate>) {
        return "null";
      } else {
        return "<value>";
      }
    }, v);
  }

  std::string PropertiesDebugString(const std::vector<std::pair<String, graph::Value>>& props) {
    return std::format(
      "{{{}}}",
      Join(props, ", ", [](const auto& kv) {
        return std::format("{}: {}", kv.first, ValueDebugString(kv.second));
      })
    );
  }

  std::string LabelsDebugString(const std::vector<String>& labels) {
    return std::format(
      "[{}]",
      Join(labels, ", ", [](const auto& s) {
        return std::format("\"{}\"", s);
      })
    );
  }

  std::string OptionalLabelsDebugString(const std::optional<std::vector<String>>& labels) {
    if (!labels.has_value()) {
      return "";
    }
    return std::format("labels={}", LabelsDebugString(*labels));
  }

  std::string OptionalPropertiesDebugString(
    const std::optional<std::vector<std::pair<String, graph::Value>>>& props
  ) {
    if (!props.has_value()) {
      return "";
    }
    return std::format("properties={}", PropertiesDebugString(*props));
  }

  std::string CreateNodeSpecDebugString(const graph::logical::CreateNodeSpec& spec) {
    return std::format(
      "Node(labels={}, properties={})",
      LabelsDebugString(spec.labels),
      PropertiesDebugString(spec.properties)
    );
  }

  std::string CreateEdgeSpecDebugString(const graph::logical::CreateEdgeSpec& spec) {
    std::string dir;
    switch (spec.direction) {
    case ast::EdgeDirection::Right:      dir = "->"; break;
    case ast::EdgeDirection::Left:       dir = "<-"; break;
    case ast::EdgeDirection::Undirected: dir = "--"; break;
    }

    return std::format(
      "Edge(src={}, dst={}, dir={}, label=\"{}\", properties={})",
      spec.src_alias,
      spec.dst_alias,
      dir,
      spec.edge_type,
      PropertiesDebugString(spec.properties)
    );
  }

  std::string ReturnItemDebugString(const ast::ReturnItem& item) {
    if (item.item.index() == 0) {
      return std::get<0>(item.item);
    }
    const auto& prop = std::get<1>(item.item);
    return std::format("{}.{}", prop.alias, prop.property);
  }
}

namespace graph::logical {
  String LogicalOpUnaryChild::SubtreeDebugString() const {
    return DebugString() + "\n" + IndentBlock(child->SubtreeDebugString());
  }

  String LogicalOpBinaryChild::SubtreeDebugString() const {
    return DebugString() + "\n"
         + "1)\n" + IndentBlock(left->SubtreeDebugString()) + "\n"
         + "2)\n" + IndentBlock(right->SubtreeDebugString());
  }

  String LogicalScan::DebugString() const {
    String ans = "LogicalScan(as=" + dst_alias;

    if (!labels.empty()) {
      ans += ", labels=[" + Join(labels, ", ", [](const String& s) { return s; }) + "]";
    }
    if (!property_filters.empty()) {
      ans += ", properties={" + PlannerUtils::ConcatProperties(property_filters) + "}";
    }

    ans += ")";
    return ans;
  }

  String LogicalExpand::DebugString() const {
    String ans = "LogicalExpand(";
    ans += src_alias + PlannerUtils::EdgeStrByDirection(direction) + dst_alias;

    if (!edge_alias.empty()) {
      ans += ", edge_as=" + edge_alias;
    }
    if (edge_type.has_value()) {
      ans += ", edge_label= " + edge_type.value();
    }
    if (!dst_vertex_labels.empty()) {
      ans += ", dst_labels=[" + Join(dst_vertex_labels, ", ", [](const String& s) { return s; }) + "]";
    }

    ans += ")";
    return ans;
  }

  String LogicalFilter::DebugString() const {
    return "LogicalFilter(" + (predicate ? predicate->DebugString() : "<null>") + ")";
  }

  String LogicalProject::DebugString() const {
    return "Project(" + Join(items, ", ", [](const auto& cur) -> String {
      return cur.DebugString();
    }) + ")";
  }

  String LogicalLimit::DebugString() const {
    return "Limit(" + std::to_string(limit_size) + ")";
  }

  String LogicalSort::DebugString() const {
    return "Sort(" + Join(keys, ", ", [](const ast::OrderItem& k) {
      return k.DebugString() + " " +
             (k.direction == ast::OrderDirection::Asc ? "ASC" : "DESC");
    }) + ")";
  }

  String LogicalJoin::DebugString() const {
    if (predicate != nullptr) {
      return "Join(on=" + predicate->DebugString() + ")";
    }
    return "Join(cross)";
  }

  String LogicalSet::DebugString() const {
    return "LogicalSet(" + assignment.alias +
           ", key=" + assignment.key +
           ", value=" + PlannerUtils::toString(assignment.value) + ")";
  }

  String LogicalDelete::DebugString() const {
    return "LogicalDelete(" + Join(target, ", ", [](const String& s) { return s; }) + ")";
  }

  String CreateNodeSpec::DebugString() const {
    String ans = "NodeCreate(";
    ans += PlannerUtils::ConcatStrVector(labels);
    if (!properties.empty()) {
      ans += ", " + PlannerUtils::ConcatProperties(properties);
    }
    ans += ")";
    return ans;
  }

  String CreateEdgeSpec::DebugString() const {
    String ans = "EdgeCreate(";
    ans += src_alias + PlannerUtils::EdgeStrByDirection(direction) + dst_alias;
    ans += ", " + edge_type;
    if (!properties.empty()) {
      ans += ", " + PlannerUtils::ConcatProperties(properties);
    }
    ans += ")";
    return ans;
  }

  String LogicalCreate::DebugString() const {
    return "LogicalCreate(" + Join(items, ", ", [](const auto& pattern) {
      return std::visit([](const auto& spec) {
        return spec.DebugString();
      }, pattern);
    }) + ")";
  }
}

namespace graph::exec {
  String PhysicalOpUnaryChild::SubtreeDebugString() const {
    return DebugString() + "\n" + IndentBlock(child->SubtreeDebugString());
  }

  String PhysicalOpBinaryChild::SubtreeDebugString() const {
    return DebugString() + "\n"
         + "1)\n" + IndentBlock(left->SubtreeDebugString()) + "\n"
         + "2)\n" + IndentBlock(right->SubtreeDebugString());
  }

  String LabelIndexSeekOp::DebugString() const {
    return std::format("LabelIndexSeek(label=\"{}\", as={})", label, out_alias);
  }

  String NodeScanOp::DebugString() const {
    return std::format("NodeScan(as={})", out_alias);
  }

  template <bool edge_outgoing>
  String ExpandOp<edge_outgoing>::DebugString() const {
    return std::format(
      "Expand({} {} {}, type={})",
      src_alias,
      (edge_outgoing ? "-" : "<"),
      dst_edge_alias,
      (edge_outgoing ? ">" : "-"),
      dst_node_alias,
      (edge_type.has_value() ? std::format("\"{}\"", edge_type.value()) : "*")
    );
  }

  String FilterOp::DebugString() const {
    return std::format(
      "Filter({})",
      predicate ? predicate->DebugString() : "<null>"
    );
  }

  String ProjectOp::DebugString() const {
    return std::format(
      "Project({})",
      Join(items, ", ", [](const auto& item) {
        return ReturnItemDebugString(item);
      })
    );
  }

  String LimitOp::DebugString() const {
    return std::format("Limit({})", limit_size);
  }

  String NestedLoopJoinOp::DebugString() const {
    if (predicate) {
      return std::format("NestedLoopJoin(on={})", predicate->DebugString());
    }
    return "NestedLoopJoin(cross)";
  }

  String KeyHashJoinOp::DebugString() const {
    return std::format(
      "HashJoin(on {}.{} = {}.{})",
      left_alias, left_feature_key,
      right_alias, right_feature_key
    );
  }

  String PhysicalSetOp::DebugString() const {
    std::string ans = "Set(";
    for (size_t i = 0; i < aliases.size(); ++i) {
      if (i) {
        ans += ", ";
      }
      ans += aliases[i];
      ans += " {";

      bool first = true;
      if (!labels[i].empty()) {
        ans += "labels=" + LabelsDebugString(labels[i]);
        first = false;
      }
      if (!properties[i].empty()) {
        if (!first) {
          ans += ", ";
        }
        ans += "properties=" + PropertiesDebugString(properties[i]);
      }

      ans += "}";
    }
    ans += ")";
    return ans;
  }

  String PhysicalCreateOp::DebugString() const {
    return std::format(
      "Create({})",
      Join(items, ", ", [](const auto& item) {
        return std::visit([](const auto& spec) {
          using T = std::decay_t<decltype(spec)>;
          if constexpr (std::is_same_v<T, logical::CreateNodeSpec>) {
            return CreateNodeSpecDebugString(spec);
          } else {
            return CreateEdgeSpecDebugString(spec);
          }
        }, item);
      })
    );
  }

  String PhysicalDeleteOp::DebugString() const {
    return std::format(
      "Delete({})",
      Join(aliases, ", ", [](const auto& a) {
        return a;
      })
    );
  }

}