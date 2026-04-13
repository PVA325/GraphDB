#pragma once

#include <string>
#include <variant>
#include <vector>

#include "value.hpp"

namespace ast {

using PropertyMap = std::vector<std::pair<std::string, Literal>>;

// Node pattern in MATCH clause
struct NodePattern {
  std::string alias;
  std::vector<std::string> labels;
  PropertyMap properties;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

// Edge direction
enum class EdgeDirection {
  Left,
  Right,
  Undirected
};

// Edge pattern between nodes
struct MatchEdgePattern {
  std::string alias;
  std::string label;
  PropertyMap properties;
  EdgeDirection direction;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

using MatchItem = std::variant<NodePattern, MatchEdgePattern>;

// Node or Edge pattern element
struct PatternElement {
  MatchItem element;

  bool IsNode() const;
  bool IsEdge() const;
  const NodePattern& AsNode() const;
  const MatchEdgePattern& AsEdge() const;

  std::string DebugString() const;
};

// Full graph pattern
struct Pattern {
  std::vector<PatternElement> elements;

  std::string DebugString() const;
};

// Node reference in CREATE clause
struct CreateNodeRef {
  std::string alias;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

// CREATE edge pattern
struct CreateEdgePattern {
  CreateNodeRef left_node;
  std::string alias;
  std::string label;
  PropertyMap properties;
  EdgeDirection direction;
  CreateNodeRef right_node;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

using CreateItem = std::variant<NodePattern, CreateEdgePattern>;

} // namespace ast
