#include "storage.hpp"

namespace storage {
  bool AllNodesCursor::next(Node*& out) {
    while (index_ < db_->nodes_.size()) {
      if (limit_ && returned_ >= limit_) return false;

      Node &node = db_->nodes_[index_++];
      if (node.alive) {
        if (predicate_ && !predicate_(&node)) {
          continue;
        }
        out = &node;
        returned_++;
        return true;
      }
    }
    return false;
  }
  NodeCursor::NodeCursor(storage::GraphDB *db,
                         boost::intrusive_ptr<NodeIdList> node_ids,
                         std::function<bool(Node*)> predicate,
                         size_t limit)
      : Cursor<Node, NodeId>(db, node_ids, predicate, limit) {}

  Node* NodeCursor::get_from_db(NodeId id) {
    return db_->get_node(id);
  }

  EdgeCursor::EdgeCursor(
    GraphDB* db,
    boost::intrusive_ptr<EdgeIdList> edge_ids,
    std::function<bool(Edge*)> predicate,
    size_t limit)
    : Cursor<Edge, EdgeId>(db, edge_ids, predicate, limit) {}

  Edge* EdgeCursor::get_from_db(EdgeId id) {
    return db_->get_edge(id);
  }

  NodeCursor::NodeCursor(GraphDB* db, std::vector<NodeId>&& node_ids,
                         std::function<bool(Node*)> predicate, size_t limit)
    : Cursor<Node, NodeId>(db, std::move(node_ids), predicate, limit) {}

  EdgeCursor::EdgeCursor(GraphDB* db, std::vector<EdgeId>&& edge_ids,
                         std::function<bool(Edge*)> predicate, size_t limit)
    : Cursor<Edge, EdgeId>(db, std::move(edge_ids), predicate, limit) {}
} // namespace