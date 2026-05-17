#include "storage/cursor.hpp"
#include "storage/graph_db.hpp"

namespace storage {
  NodeCursor::NodeCursor(GraphDB* db, const std::vector<NodeId>& ids,
                         std::function<bool(Node*)> predicate, size_t limit)
    : Cursor<Node, NodeId>(db, ids, predicate, limit) {}

  [[nodiscard]] Node* NodeCursor::get_from_db(NodeId id) {
    return db_->get_node(id);
  }

  EdgeCursor::EdgeCursor(GraphDB* db, const std::vector<EdgeId>& ids,
                         std::function<bool(Edge*)> predicate, size_t limit)
    : Cursor<Edge, EdgeId>(db, ids, predicate, limit) {}

  Edge* EdgeCursor::get_from_db(EdgeId id) {
    return db_->get_edge(id);
  }

  bool AllNodesCursor::next(Node*& out) {
    if (disk_mode_) {
      while (index_ < total_slots_) {
        if (limit_ && returned_ >= limit_) return false;
        Node* node = db_->get_node(index_++);
        if (!node) continue;
        if (predicate_ && !predicate_(node)) continue;
        out = node;
        ++returned_;
        return true;
      }
      return false;
    }

    while (index_ < db_->nodes_ram_.size()) {
      if (limit_ && returned_ >= limit_) return false;
      Node& node = db_->nodes_ram_[index_++];
      if (!node.alive) continue;
      if (predicate_ && !predicate_(&node)) continue;
      out = &node;
      ++returned_;
      return true;
    }
    return false;
  }

  CursorFactory::CursorFactory(GraphDB* db, NodeIndex* node_index, EdgeIndex* edge_index)
    : db_(db), node_index_(node_index), edge_index_(edge_index) {}

  std::unique_ptr<NodeCursor> CursorFactory::all_nodes(
    std::function<bool(Node*)> predicate, size_t limit) {
    return std::unique_ptr<NodeCursor>(new AllNodesCursor(db_, predicate, limit));
  }

  std::unique_ptr<NodeCursor> CursorFactory::nodes_with_label(
    const std::string& label,
    std::function<bool(Node*)> predicate, size_t limit) {

    const auto& list = node_index_->by_label(label);
    if (list.empty()) {
      return std::make_unique<NodeCursor>(db_, std::vector<NodeId>{}, predicate, limit);
    }
    return std::make_unique<NodeCursor>(db_, list, predicate, limit);
  }

  std::unique_ptr<NodeCursor> CursorFactory::nodes_with_property(
    const std::string& key, const Value& val,
    std::function<bool(Node*)> predicate, size_t limit) {

    const auto& list = node_index_->by_property(key, val);
    if (list.empty()) {
      return std::make_unique<NodeCursor>(db_, std::vector<NodeId>{}, predicate, limit);
    }
    return std::make_unique<NodeCursor>(db_, list, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> CursorFactory::outgoing_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate, size_t limit) {

    const auto& list = edge_index_->outgoing(node_id);
    if (list.empty()) {
      return std::make_unique<EdgeCursor>(db_, std::vector<EdgeId>{}, predicate, limit);
    }
    return std::make_unique<EdgeCursor>(db_, list, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> CursorFactory::incoming_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate, size_t limit) {

    const auto& list = edge_index_->incoming(node_id);
    if (list.empty()) {
      return std::make_unique<EdgeCursor>(db_, std::vector<EdgeId>{}, predicate, limit);
    }
    return std::make_unique<EdgeCursor>(db_, list, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> CursorFactory::edges_by_type(
    const std::string& type,
    std::function<bool(Edge*)> predicate, size_t limit) {

    const auto& list = edge_index_->by_type(type);
    if (list.empty()) {
      return std::make_unique<EdgeCursor>(db_, std::vector<EdgeId>{}, predicate, limit);
    }
    return std::make_unique<EdgeCursor>(db_, list, predicate, limit);
  }

} // namespace storage