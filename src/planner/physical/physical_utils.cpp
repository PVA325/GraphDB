#include "planner/query_planner.hpp"

namespace graph::exec {

Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key) {
  if (std::holds_alternative<Value>(slot.value)) {
    if (!feature_key.empty()) {
      throw std::runtime_error("Error during KeyHashJoin: invalid alias or property");
    }
    return std::get<2>(slot.value);
  }
  if (std::holds_alternative<Edge*>(slot.value)) {
    return std::get<Edge*>(slot.value)->properties[feature_key];
  }
  return std::get<Node*>(slot.value)->properties[feature_key];
}

Row MergeRows(Row& first, Row& second) {
  static auto check_vectors_intersects = [](const std::vector<String>& f, const std::vector<String>& s) {
    return std::any_of(f.begin(), f.end(), [&s](const String& str) {
      return std::find(s.begin(), s.end(), str) != s.end();
    });
  };
#ifndef NDEBUG
  assert(!check_vectors_intersects(first.slots_names, second.slots_names));
#endif
  Row ans;
  ans.slots.insert(ans.slots.end(), first.slots.begin(), first.slots.end());
  ans.slots.insert(ans.slots.end(), second.slots.begin(), second.slots.end());

  ans.slots_names.insert(ans.slots_names.end(), first.slots_names.begin(), first.slots_names.end());
  ans.slots_names.insert(ans.slots_names.end(), second.slots_names.begin(), second.slots_names.end());

  for (size_t i = 0; i < ans.slots_names.size(); ++i) {
    ans.slots_mapping.add_map(ans.slots_names[i], i);
  }

  return ans;
}

}