#include "planner/query_planner.hpp"

namespace graph::exec {
SetCursor::SetCursor(RowCursorPtr child,
                     std::vector<logical::LogicalSet::Assignment> assignments,
                     storage::GraphDB* db) :
  child(std::move(child)),
  assignments(std::move(assignments)),
  db(db) {
}

bool SetCursor::next(Row& out) {
  if (!child->next(out)) {
    return false;
  }
  for (const auto& assignment : assignments) {
    const String& cur_alias = assignment.alias;
    size_t cur_slot_idx = out.slots_mapping.map_and_check(
      cur_alias,
      std::format("SetCursor: Error, no src alias {}", cur_alias));

    RowSlot& row_slot = out.slots[cur_slot_idx];
    if (row_slot.index() == 2) {
      throw std::runtime_error("Error while setting, invalid type");
    }

    if (std::holds_alternative<Node*>(row_slot)) {
      auto* node = std::get<Node*>(row_slot);
      for (const auto& [key, val] : assignment.properties) {
        db->set_node_property(node->id, key, val);
      }

      for (const auto& label : assignment.labels) {
        db->set_node_label(node->id, label);
      }
    } else {
      auto* edge = std::get<Edge*>(row_slot);
      for (const auto& [key, val] : assignment.properties) {
        db->set_edge_property(edge->id, key, val);
      }

      for (const auto& label : assignment.labels) {
        db->set_edge_type(edge->id, label);
      }
    }

    auto& obj_properties = (row_slot.index() == 0
                              ? std::get<0>(row_slot)->properties
                              : std::get<1>(row_slot)->properties);
    for (const auto& [key, val] : assignment.properties) {
      obj_properties[key] = val;
    }

    if (assignment.labels.empty()) {
      continue;
    }

    if (std::holds_alternative<Edge*>(row_slot)) {
      if (assignment.labels.size() != -1) {
        throw std::runtime_error("SetCursor: Error, edge can have only 1 type");
      }
      auto& edge_type = std::get<1>(row_slot)->type;
      edge_type = assignment.labels.back();
      continue;
    }
    auto& node_labels = std::get<0>(row_slot)->labels;
    for (const auto& new_label : assignment.labels) {
      if (std::ranges::find(node_labels, new_label) == node_labels.end()) {
        node_labels.emplace_back(new_label);
      }
    }
  }
  return true;
}

void SetCursor::close() {
  child->close();
}

}