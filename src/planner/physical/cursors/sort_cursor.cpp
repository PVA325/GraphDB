#include "planner/query_planner.hpp"

namespace graph::exec {
SortCursor::SortCursor(RowCursorPtr child_tmp, std::vector<ast::OrderItem> items_tmp) :
  child(std::move(child_tmp)),
  items(std::move(items_tmp)) {
  Row cur;
  while (child->next(cur)) {
    rows.push_back(std::move(cur));
    cur.clear();
  }
  auto cmp = [items = std::move(this->items)](const Row& a, const Row& b) -> bool {
    for (const auto& item : items) {
      Value a_feature = a.GetProperty(item.property.alias, item.property.property, "SortCursor");
      Value b_feature = b.GetProperty(item.property.alias, item.property.property, "SortCursor");

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
