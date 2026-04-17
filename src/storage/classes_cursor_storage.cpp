#include "storage.hpp"

namespace storage {
  NodeCursor::NodeCursor(storage::GraphDB *db,
                         const std::vector<storage::NodeId>& node_ids,
                         std::function<bool(storage::Node *)> predicate,
                         size_t limit)
      : Cursor<Node, NodeId>(db, node_ids, predicate, limit) {}

  Node* NodeCursor::get_from_db(NodeId id) {
    return db_->get_node(id);
  }

  EdgeCursor::EdgeCursor(
    GraphDB* db,
    const std::vector<EdgeId>& edge_ids,
    std::function<bool(Edge*)> predicate,
    size_t limit)
    : Cursor<Edge, EdgeId>(db, edge_ids, predicate, limit) {}

  Edge* EdgeCursor::get_from_db(EdgeId id) {
    return db_->get_edge(id);
  }


} // namespace