#ifndef GRAPHDB_NODE_SCAN_CURSOR_HPP
#define GRAPHDB_NODE_SCAN_CURSOR_HPP

#include "row_cursor.hpp"
#include "storage/storage.hpp" /// TODO

namespace graph::exec {
struct NodeScanCursor : RowCursor {
  std::unique_ptr<storage::NodeCursor> nodes_cursor;
  String out_alias;

  NodeScanCursor(NodeScanCursor&&) = default;

  NodeScanCursor(std::unique_ptr<storage::NodeCursor> nodes_cursor, String out_alias);

  bool next(Row& out) override;

  void close() override;

  ~NodeScanCursor() override = default;
};

}
#endif //GRAPHDB_NODE_SCAN_CURSOR_HPP