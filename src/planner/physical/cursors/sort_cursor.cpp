#include "planner/query_planner.hpp"

namespace graph::exec {
SortCursor::SortCursor(RowCursorPtr child, std::vector<ast::OrderItem> items) :
  child(std::move(child)),
  items(std::move(items)) {
  Row cur;
  while (child->next(cur)) {
    rows.push_back(std::move(cur));
  }
  auto cmp = [items = std::move(this->items)](const Row& a, const Row& b) -> bool {
    for (const auto& item : items) {
      String err = std::format("SortCursor: Error, no such alias {}", item.property.alias);
      size_t a_idx = a.slots_mapping.map_and_check(item.property.alias, err);
      size_t b_idx = b.slots_mapping.map_and_check(item.property.alias, err);

      Value a_feature = GetFeatureFromSlot(a.slots[a_idx], item.property.property);
      Value b_feature = GetFeatureFromSlot(a.slots[a_idx], item.property.property);

      if (a_feature.index() != b_feature.index()) {
        throw std::runtime_error("SortCursor: Error type mistmatch");
      }

      if (a_feature != b_feature) {
        bool is_less = std::visit([](auto&& a, auto&& b) -> bool {
          using A = std::decay_t<decltype(a)>;
          using B = std::decay_t<decltype(b)>;

          if constexpr (std::is_same_v<A, B>) {
            return a < b;
          }
          return false;
        }, a_feature, b_feature);
        return (item.direction == ast::OrderDirection::Asc ? is_less : !is_less);
      }
    }
    return true;
  };
  std::sort(rows.begin(), rows.end(), cmp);
  std::reverse(rows.begin(), rows.end());
}

bool SortCursor::next(Row& out) {
  if (rows.empty()) {
    return false;
  }
  out = std::move(rows.back());
  rows.pop_back();
  return true;
}

void SortCursor::close() {
  child->close();
}

}