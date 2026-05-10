#ifndef GRAPHDB_PHYSICAL_UTILS_HPP
#define GRAPHDB_PHYSICAL_UTILS_HPP
#include "common/common_value.hpp"

namespace graph::exec {
std::tuple<Row, bool> MergeRows(const Row& first, const Row& second);
Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key);
}  // namespace graph::exec

#endif //GRAPHDB_PHYSICAL_UTILS_HPP