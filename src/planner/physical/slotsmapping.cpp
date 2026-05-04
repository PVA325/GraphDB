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

}