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

std::tuple<Row, bool> MergeRows(const Row& first, const Row& second) {
  Row ans = first;

  for (size_t i = 0; i < second.slots_names.size(); ++i) {
    const auto& name = second.slots_names[i];

    if (ans.slots_mapping.key_exists(name)) {
      size_t pos = ans.slots_mapping.map(name);
      if (ans.slots[pos] != second.slots[i]) {
        return {Row{}, false};
      }
    } else {
      ans.slots.emplace_back(second.slots[i]);
      ans.slots_names.emplace_back(name);
      ans.slots_mapping.add_map(ans.slots_names.back(), ans.slots.size() - 1);
    }
  }

  return {std::move(ans), true};
}

}