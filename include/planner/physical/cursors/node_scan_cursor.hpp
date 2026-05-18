#pragma once

#include "row_cursor.hpp"
#include "storage/storage.hpp" /// TODO

namespace graph::exec {

struct NodeScanCursor : RowCursor {
  NodeScanCursor(NodeScanCursor&&) = default;

  NodeScanCursor(std::unique_ptr<storage::NodeCursor> nodes_cursor, String out_alias);

  bool next(Row& out) override;

  void close() override;

  ~NodeScanCursor() override = default;

public:
  std::unique_ptr<storage::NodeCursor> nodes_cursor;
  String out_alias;
};

} // namespace graph::exec

