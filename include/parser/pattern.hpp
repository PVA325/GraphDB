#pragma once

#include <string>
#include <variant>
#include <vector>

#include "value.hpp"
#include "common/common_value.hpp"

namespace ast {

using PropertyMap = std::vector<std::pair<std::string, Literal>>;

// Node pattern in MATCH clause.
struct NodePattern {
  [[nodiscard]] std::string DebugString() const;

  std::string alias;
  std::vector<std::string> labels;
  PropertyMap properties;

  size_t line = 0;
  size_t col = 0;
};

// Edge pattern between nodes.
struct MatchEdgePattern {
  [[nodiscard]] std::string DebugString() const;

  std::string alias;
  std::string label;
  PropertyMap properties;
  EdgeDirection direction;

  size_t line = 0;
  size_t col = 0;
};

using MatchItem = std::variant<NodePattern, MatchEdgePattern>;

// Node or Edge pattern element.
struct PatternElement {
  [[nodiscard]] bool IsNode() const;
  [[nodiscard]] bool IsEdge() const;
  [[nodiscard]] const NodePattern& AsNode() const;
  [[nodiscard]] const MatchEdgePattern& AsEdge() const;

  [[nodiscard]] std::string DebugString() const;

  MatchItem element;
};

// Full graph pattern.
struct Pattern {
  [[nodiscard]] std::string DebugString() const;

  std::vector<PatternElement> elements;
};

// Node reference in CREATE clause.
struct CreateNodeRef {
  [[nodiscard]] std::string DebugString() const;

  std::string alias;

  size_t line = 0;
  size_t col = 0;
};

// CREATE edge pattern.
struct CreateEdgePattern {
  [[nodiscard]] std::string DebugString() const;

  CreateNodeRef left_node;
  std::string alias;
  std::string label;
  PropertyMap properties;
  EdgeDirection direction;
  CreateNodeRef right_node;

  size_t line = 0;
  size_t col = 0;
};

} // namespace ast
