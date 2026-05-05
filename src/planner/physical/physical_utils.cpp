#include "planner/query_planner.hpp"

namespace graph::exec {

Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key) {
  if (slot.index() == 2) {
    if (!feature_key.empty()) {
      throw std::runtime_error("Error during KeyHashJoin: invalid alias or property");
    }
    return std::get<2>(slot);
  }
  if (slot.index() == 1) {
    return std::get<1>(slot)->properties[feature_key];
  }
  return std::get<0>(slot)->properties[feature_key];
}

Row MergeRows(Row& first, Row& second) {
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