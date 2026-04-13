#include "storage.hpp"

namespace storage {
  std::unique_ptr<NodeCursor> GraphDB::all_nodes(
    std::function<bool(Node*)> predicate,
    size_t limit) {

    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());

    for (NodeId i = 0; i < nodes_.size(); ++i) {
      ids.push_back(i);
    }

    return std::make_unique<NodeCursor>(this, ids, predicate, limit);
  }

  std::unique_ptr<NodeCursor> GraphDB::nodes_with_label(
    const std::string& label,
    std::function<bool(Node*)> predicate,
    size_t limit) {

    auto it = label_index_.find(label);

    if (it == label_index_.end()) {
      return std::make_unique<NodeCursor>(
        this,
        std::vector<NodeId>{},
        predicate,
        limit
      );
    }

    return std::make_unique<NodeCursor>(
      this,
      it->second,
      predicate,
      limit
    );
  }

  std::unique_ptr<NodeCursor> GraphDB::nodes_with_property(
    const std::string& key,
    const Value& val,
    std::function<bool(Node*)> predicate,
    size_t limit) {

    auto kit = property_index_.find(key);

    if (kit == property_index_.end()) {
      return std::make_unique<NodeCursor>(
        this,
        std::vector<NodeId>{},
        predicate,
        limit
      );
    }

    auto vit = kit->second.find(val);

    if (vit == kit->second.end()) {
      return std::make_unique<NodeCursor>(
        this,
        std::vector<NodeId>{},
        predicate,
        limit
      );
    }

    return std::make_unique<NodeCursor>(
      this,
      vit->second,
      predicate,
      limit
    );
  }


  std::unique_ptr<EdgeCursor> GraphDB::outgoing_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate,
    size_t limit) {

    auto it = outgoing_.find(node_id);

    if (it == outgoing_.end()) {
      return std::make_unique<EdgeCursor>(
        this,
        std::vector<EdgeId>{},
        predicate,
        limit
      );
    }

    return std::make_unique<EdgeCursor>(
      this,
      it->second,
      predicate,
      limit
    );
  }


  std::unique_ptr<EdgeCursor> GraphDB::incoming_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate,
    size_t limit) {

    auto it = incoming_.find(node_id);

    if (it == incoming_.end()) {
      return std::make_unique<EdgeCursor>(
        this,
        std::vector<EdgeId>{},
        predicate,
        limit
      );
    }

    return std::make_unique<EdgeCursor>(
      this,
      it->second,
      predicate,
      limit
    );
  }


  std::unique_ptr<EdgeCursor> GraphDB::edges_by_type(
    const std::string& type,
    std::function<bool(Edge*)> predicate,
    size_t limit) {

    auto it = edge_type_index_.find(type);

    if (it == edge_type_index_.end()) {
      return std::make_unique<EdgeCursor>(
        this,
        std::vector<EdgeId>{},
        predicate,
        limit
      );
    }

    return std::make_unique<EdgeCursor>(
      this,
      it->second,
      predicate,
      limit
    );
  }
} // namespace