#pragma once

#include "common/common_value.hpp"

namespace graph::exec {

std::tuple<Row, bool> MergeRows(const Row& first, const Row& second);
Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key);

}  // namespace graph::exec
