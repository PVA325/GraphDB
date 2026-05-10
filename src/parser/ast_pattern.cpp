#include "parser/parser.hpp"

#include <utility>

namespace ast {

// Checks if the pattern element is a node.
bool PatternElement::IsNode() const {
  return std::holds_alternative<NodePattern>(element);
}

// Checks if the pattern element is an edge.
bool PatternElement::IsEdge() const {
  return std::holds_alternative<MatchEdgePattern>(element);
}

// Gets the node pattern from the pattern element.
const NodePattern& PatternElement::AsNode() const {
  return std::get<NodePattern>(element);
}

// Gets the edge pattern from the pattern element.
const MatchEdgePattern& PatternElement::AsEdge() const {
  return std::get<MatchEdgePattern>(element);
}

}  // namespace ast