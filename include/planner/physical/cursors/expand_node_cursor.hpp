#pragma once

#include "common/common_value.hpp"
#include "storage/storage.hpp"

namespace graph::exec {

template <bool edge_outgoing>
struct ExpandNodeCursorPhysical : RowCursor {
  enum class Direction { kOutgoing, kIngoing };

  RowCursorPtr child_cursor;
  std::unique_ptr<storage::EdgeCursor> edge_cursor;
  std::function<bool(Edge*)> label_predicate;
  String src_node_alias;
  String dst_edge_alias;
  String dst_node_alias;
  storage::GraphDB* db;

  Row last_child_row;

  ExpandNodeCursorPhysical(const ExpandNodeCursorPhysical&) = delete;

  ExpandNodeCursorPhysical(ExpandNodeCursorPhysical&&) = default;

  ExpandNodeCursorPhysical(RowCursorPtr child_cursor, String src_node_alias, String dst_edge_alias,
                           String dst_node_alias, std::function<bool(Edge*)> label_predicate,
                           storage::GraphDB* db);

  bool next(Row& out) override;

  void close() override;

  ~ExpandNodeCursorPhysical() override = default;
};

} // namespace graph::exec

#include "expand_node_cursor.tpp"

