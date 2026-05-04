#include <cmath>

#include "planner/query_planner.hpp"


graph::String graph::PlannerUtils::EdgeStrByDirection(ast::EdgeDirection dir) {
  if (dir == ast::EdgeDirection::Right) {
    return "->";
  }
  if (dir == ast::EdgeDirection::Left) {
    return "<-";
  }
  return "-";
}

graph::String graph::PlannerUtils::ValueToString(const graph::Value& val) {
  const auto visitor = overloads{
    [](Int cur) -> String { return std::to_string(cur); },
    [](Double cur) -> String { return std::to_string(cur); },
    [](Bool cur) -> String { return (cur ? "true" : "false"); },
    [](String cur) -> String { return std::move(cur); }
  };
  return std::visit(visitor, val);
}

bool graph::PlannerUtils::ValueToBool(graph::Value val) {
  const auto visitor = overloads{
    [](Int cur) -> bool { return cur; },
    [](Double cur) -> bool { return cur != 0.0; },
    [](Bool cur) -> bool { return cur; },
    [](const String& cur) -> bool { return (!cur.empty()); }
  };
  return std::visit(visitor, val);
}

graph::String graph::PlannerUtils::ConcatProperties(const std::vector<std::pair<String, Value>>& v) {
  String ans;
  for (const auto& cur : v) {
    if (!ans.empty()) {
      ans += ", ";
    }
    ans += cur.first + ": " + ValueToString(cur.second);
  }
  return ans;
}

graph::String graph::PlannerUtils::ConcatStrVector(const std::vector<String>& v) {
  String ans;
  for (const auto& cur : v) {
    ans += cur;
  }
  return ans;
}
