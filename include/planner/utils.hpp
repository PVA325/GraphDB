#ifndef GRAPHDB_UTILS_HPP
#define GRAPHDB_UTILS_HPP
#include "common/common_value.hpp"

namespace graph::PlannerUtils {
template <typename... Types>
struct overloads : Types... {
  using Types::operator()...;
};

template<typename T>
T* GetUniqPtrVal(const std::unique_ptr<T>& ptr) {
  return (ptr ? ptr.get() : nullptr);
}

String ValueToString(const Value& val);
String ConcatStrVector(const std::vector<String>& v);
String ConcatProperties(const std::vector<std::pair<String, Value>>& v);
String EdgeStrByDirection(ast::EdgeDirection dir);

template <typename PropertyM>
  requires(std::is_same_v<std::decay_t<PropertyM>, ast::PropertyMap>)
void transferProperties(
  std::vector<std::pair<String, Value>>& prop,
  PropertyM&& other_prop) {
  if (!prop.empty()) {
    throw std::runtime_error("Properties transfer invariant violation");
  }
  prop.reserve(other_prop.size());
  for (size_t i = 0; i < other_prop.size(); ++i) {
    if constexpr (std::is_lvalue_reference_v<PropertyM>) {
      prop.emplace_back(
        other_prop[i].first,
        other_prop[i].second.value
      );
    } else {
      prop.emplace_back(
        std::move(other_prop[i].first),
        std::move(other_prop[i].second.value)
      );
    }
  }
}

bool ValueToBool(Value val);

template<typename T>
std::vector<T> MergeVectors(std::vector<T>&& l, std::vector<T>&& r) {
  std::vector<T> res = std::move(l);
  res.insert(res.end(), std::make_move_iterator(r.begin()), std::make_move_iterator(r.end()));
  return res;
}
} // namespace graph::PlannerUtils

#endif //GRAPHDB_UTILS_HPP