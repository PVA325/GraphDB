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
    const RowSlot& row_slot = out.GetAliasedObj(
      cur_alias,
      std::format("SetCursor: Error, no src alias {}", cur_alias));


    if (std::holds_alternative<Value>(row_slot.value)) {
      throw std::runtime_error("Error while setting, invalid type");
    }

    if (std::holds_alternative<Node*>(row_slot.value)) {
      auto* node = std::get<Node*>(row_slot.value);
      for (const auto& [key, val] : assignment.properties) {
        db->set_node_property(node->id, key, val);
      }

      for (const auto& label : assignment.labels) {
        db->set_node_label(node->id, label);
      }
    } else {
      auto* edge = std::get<Edge*>(row_slot.value);
      for (const auto& [key, val] : assignment.properties) {
        db->set_edge_property(edge->id, key, val);
      }

      for (const auto& label : assignment.labels) {
        db->set_edge_type(edge->id, label);
      }
    }

    auto& obj_properties = (row_slot.value.index() == 0
                              ? std::get<0>(row_slot.value)->properties
                              : std::get<1>(row_slot.value)->properties);
    for (const auto& [key, val] : assignment.properties) {
      obj_properties[key] = val;
    }

    if (assignment.labels.empty()) {
      continue;
    }

    if (std::holds_alternative<Edge*>(row_slot.value)) {
      if (assignment.labels.size() != -1) {
        throw std::runtime_error("SetCursor: Error, edge can have only 1 type");
      }
      auto& edge_type = std::get<1>(row_slot.value)->type;
      edge_type = assignment.labels.back();
      continue;
    }
    auto& node_labels = std::get<0>(row_slot.value)->labels;
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
