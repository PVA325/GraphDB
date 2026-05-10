#include "planner/query_planner.hpp"

namespace graph::exec {
bool SlotMapping::key_exists(const String& key) const {
  return alias_to_slot.contains(key);
}

size_t SlotMapping::map(const String& key) const {
  return alias_to_slot.at(key);
}

size_t SlotMapping::map_and_check(const String& key, const String& err_msg) const {
  auto it = alias_to_slot.find(key);
  if (it == alias_to_slot.end()) {
    throw std::runtime_error(err_msg);
  }
  return it->second;
}

void SlotMapping::add_map(const String& key, size_t idx) {
  alias_to_slot.emplace(key, idx);
}

Value Row::GetProperty(const std::string& alias, const std::string& property, const std::string& parent_op) const {
  if (!slots_mapping.key_exists(alias)) {
    auto slot = GetAliasedObj(alias + "." + property);
    if (!std::holds_alternative<Value>(slot.value)) {
      throw std::runtime_error(std::format("GetProperty({}): Error after Project", parent_op));
    }
    return std::get<Value>(slot.value);
  }
  auto slot = GetAliasedObj(alias);
  if (std::holds_alternative<Value>(slot.value)) {
    throw std::runtime_error(
      std::format("EvalContext({}): Error, invalid expression {} has is not a node or an edge", parent_op, alias)
    );
  }
  const auto& props = (std::holds_alternative<graph::exec::Edge*>(slot.value) ?
        std::get<graph::exec::Edge*>(slot.value)->properties
      : std::get<graph::exec::Node*>(slot.value)->properties
  );
  auto it = props.find(property);
  if (it == props.end()) {
    throw std::runtime_error(
      std::format("EvalContext: Error, alias {} has no property {}", alias, property)
    );
  }
  return it->second;
}
}
