#ifndef GRAPHDB_PHYSICAL_PLAN_HPP
#define GRAPHDB_PHYSICAL_PLAN_HPP

#include "common/common_value.hpp"
#include "physical/operations/exec_context.hpp"
#include "physical/cursors/row_cursor.hpp"
#include "physical/operations/physical_op.hpp"
#include "physical/operations/physical_op_no_child.hpp"
#include "physical/operations/physical_op_unary_child.hpp"
#include "physical/operations/physical_op_binary.hpp"
#include "physical/cursors/node_scan_cursor.hpp"
#include "physical/cursors/node_property_filter_cursor.hpp"
#include "physical/cursors/expand_node_cursor.hpp"
#include "physical/cursors/filter_cursor.hpp"
#include "physical/cursors/project_cursor.hpp"
#include "physical/cursors/limit_cursor.hpp"
#include "physical/cursors/set_cursor.hpp"
#include "physical/cursors/create_cursor.hpp"
#include "physical/cursors/delete_cursor.hpp"
#include "physical/cursors/hashjoin_cursor.hpp"
#include "physical/cursors/nesteed_loop_join_cursor.hpp"
#include "physical/cursors/sort_cursor.hpp"

#include "physical/operations/node_scan.hpp"
#include "physical/operations/label_index_seek.hpp"
#include "physical/operations/node_property_filter.hpp"
#include "physical/operations/expand.hpp"
#include "physical/operations/filter.hpp"
#include "physical/operations/project.hpp"
#include "physical/operations/limit.hpp"
#include "physical/operations/nested_loop_join.hpp"
#include "physical/operations/hashjoin.hpp"
#include "physical/operations/create.hpp"
#include "physical/operations/set.hpp"
#include "physical/operations/delete.hpp"
#include "physical/operations/sort.hpp"
#include "physical/operations/physical_plan.hpp"

namespace graph::exec {

Row MergeRows(Row& first, Row& second);
Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key);

}

#endif //GRAPHDB_PHYSICAL_PLAN_HPP