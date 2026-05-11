#include <cassert>

#include "storage/edge_manager.hpp"

namespace storage {

  EdgeManager::EdgeManager(EdgeStore*    store,
                           EdgeIndex*    index,
                           NodeStore*    node_store,
                           MetricsStore* metrics,
                           FreeList*  free_ids)
    : store_(store), index_(index), node_store_(node_store),
      metrics_(metrics), free_ids_(free_ids) {}

  EdgeId EdgeManager::create(NodeId src, NodeId dst,
                             const std::string& type,
                             const Properties& props) {
    EdgeId id = free_ids_->next(store_->slot_count());

    Edge edge;
    edge.id = id;
    edge.alive = true;
    edge.src = src;
    edge.dst = dst;
    edge.type = type;
    edge.properties = props;

    store_->put(id, edge);
    index_->add_edge(id, src, dst, type);

    std::vector<std::string> src_labels;
    Node* src_node = node_store_->get(src);
    if (src_node) { src_labels = src_node->labels; }

    metrics_->on_edge_created(src_labels);

    return id;
  }

  Edge* EdgeManager::get(EdgeId id) {
    return store_->get(id);
  }

  void EdgeManager::set_property(EdgeId id, const std::string& key, const Value& val) {
    Edge* edge = store_->get(id);
    assert(edge && edge->alive);
    edge->properties[key] = val;
    store_->put(id, *edge);
  }

  void EdgeManager::set_type(EdgeId id, const std::string& new_type) {
    Edge* edge = store_->get(id);
    assert(edge && edge->alive);

    index_->retype_edge(id, edge->type, new_type);
    edge->type = new_type;
    store_->put(id, *edge);
  }

  void EdgeManager::remove(EdgeId id) {
    Edge* edge = store_->get(id);
    assert(edge && edge->alive);

    NodeId src  = edge->src;
    NodeId dst  = edge->dst;
    std::string type = edge->type;

    std::vector<std::string> src_labels;
    Node* src_node = node_store_->get(src);
    if (src_node) { src_labels = src_node->labels; }

    index_->remove_edge(id, src, dst, type);
    metrics_->on_edge_deleted(src_labels);

    store_->remove(id);
    free_ids_->free(id);
  }

} // namespace storage