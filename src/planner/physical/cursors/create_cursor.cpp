#include "planner/query_planner.hpp"

namespace graph::exec {


CreateCursor::CreateCursor(
  RowCursorPtr child,
  std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
  storage::GraphDB* db) :
  child(std::move(child)),
  items(std::move(items)),
  db(db) {
}

bool CreateCursor::next(Row& out) {
  if (child == nullptr) {
    if (was_writing) {
      return false;
    }
    was_writing = true;
    for (const auto& create_pattern : items) {
      if (std::holds_alternative<logical::CreateNodeSpec>(create_pattern)) {
        const auto& node_spec = std::get<logical::CreateNodeSpec>(create_pattern);
        std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());

        storage::NodeId created_node = db->create_node(node_spec.labels, m);
        out.AddValue(db->get_node(created_node), node_spec.dst_alias,
                     std::format("CreateCursor: Error, alias {} already exists", node_spec.dst_alias));
      } else {
        const auto& edge_spec = std::get<logical::CreateEdgeSpec>(create_pattern);

        size_t src_idx = out.slots_mapping.map_and_check(
          edge_spec.src_alias,
          std::format("CreateCursor: Error, no src alias {} for edge creating", edge_spec.src_alias)
        );
        size_t dst_idx = out.slots_mapping.map_and_check(
          edge_spec.dst_node_alias,
          std::format("CreateCursor: Error, no dst alias {} for edge creating", edge_spec.dst_node_alias)
        );

        if (!std::holds_alternative<Node*>(out.slots[src_idx]) ||
          !std::holds_alternative<Node*>(out.slots[dst_idx])) {
          throw std::runtime_error("CreateCursor: Error, edge should be between vertexes");
        }
        const auto& src = std::get<Node*>(out.slots[src_idx]);
        const auto& dst = std::get<Node*>(out.slots[dst_idx]);

        std::unordered_map<String, Value> m(edge_spec.properties.begin(), edge_spec.properties.end());

        storage::EdgeId created_edge = db->create_edge(src->id, dst->id, edge_spec.edge_type, m);
        out.AddValue(db->get_edge(created_edge), edge_spec.edge_alias,
                     std::format("CreateCursor: Error, alias {} already exists", edge_spec.edge_alias));
      }
    }
    return true;
  }
  if (!child->next(out)) {
    return false;
  }

  for (const auto& create_pattern : items) {
    if (create_pattern.index() == 0) {
      const auto& node_spec = std::get<logical::CreateNodeSpec>(create_pattern);
      std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());
      db->create_node(node_spec.labels, m);
      continue;
    }
    const auto& edge_spec = std::get<logical::CreateEdgeSpec>(create_pattern);

    const auto& src = out.slots[
      out.slots_mapping.map_and_check(
        edge_spec.src_alias,
        std::format("CreateCursor: Error, invalid src alias {}", edge_spec.src_alias))
    ];
    const auto& dst = out.slots[
      out.slots_mapping.map_and_check(
        edge_spec.dst_node_alias,
        std::format("CreateCursor: Error, invalid src alias {}", edge_spec.dst_node_alias)
      )
    ];
    if (src.index() != 0 || dst.index() != 0) {
      throw std::runtime_error("CreateCursor: Error, edge must be between vertexes");
    }

    if (edge_spec.direction == ast::EdgeDirection::Right ||
      edge_spec.direction == ast::EdgeDirection::Undirected) {
      db->create_edge(
        std::get<0>(src)->id,
        std::get<0>(dst)->id,
        edge_spec.edge_type,
        std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end())
      );
    }
    if (edge_spec.direction == ast::EdgeDirection::Left ||
      edge_spec.direction == ast::EdgeDirection::Undirected) {
      db->create_edge(
        std::get<0>(dst)->id,
        std::get<0>(src)->id,
        edge_spec.edge_type,
        std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end())
      );
    }
  }
  return true;
}

void CreateCursor::close() {
  child->close();
}

}