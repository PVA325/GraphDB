#include <algorithm>
#include <cassert>

#include "storage/NodeEntity/node_manager.hpp"

namespace storage {

  NodeManager::NodeManager(NodeStore* store,
                           NodeIndex* index,
                           EdgeIndex* edge_index,
                           MetricsStore* metrics,
                           FreeList* free_ids)
    : store_(store), index_(index), edge_index_(edge_index),
      metrics_(metrics), free_ids_(free_ids) {}

  NodeId NodeManager::create(const std::vector<std::string>& labels,
                             const Properties& props) {
    NodeId id = free_ids_->next(store_->slot_count());

    Node node{
    .id = id,
    .alive = true,
    .labels = labels,
    .properties = props};

    store_->put(node);

    for (const auto& label : labels) {
      index_->add_label(id, label);
    }
    for (const auto& [k, v] : props) {
      index_->add_property(id, k, v);
    }

    metrics_->on_node_created(labels, props);

    return id;
  }

  Node* NodeManager::get(NodeId id) {
    return store_->get(id);
  }

  void NodeManager::set_property(NodeId id, const std::string& key, const Value& val) {
    Node* node = store_->get(id);
    assert(node && node->alive);

    auto it = node->properties.find(key);
    if (it != node->properties.end()) {
      index_->update_property(id, key, it->second, val);
    } else {
      index_->add_property(id, key, val);
    }

    node->properties[key] = val;
    store_->put(*node);
  }

  void NodeManager::add_label(NodeId id, const std::string& label) {
    Node* node = store_->get(id);
    assert(node && node->alive);

    if (std::find(node->labels.begin(), node->labels.end(), label) != node->labels.end()) {
      return;
    }

    node->labels.push_back(label);
    store_->put(*node);

    index_->add_label(id, label);

    size_t out_deg = edge_index_->out_degree(id);
    metrics_->on_label_added(label, node->properties, out_deg);
  }

  void NodeManager::remove_label(NodeId id, const std::string& label) {
    Node* node = store_->get(id);
    assert(node && node->alive);

    node->labels.erase(
      std::remove(node->labels.begin(), node->labels.end(), label),
      node->labels.end());
    store_->put(*node);

    index_->remove_label(id, label);

    size_t out_deg = edge_index_->out_degree(id);
    metrics_->on_label_removed(label, out_deg);
  }

  void NodeManager::remove(NodeId id) {
    Node* node = store_->get(id);
    assert(node && node->alive);

    std::vector<std::string> labels = node->labels;
    Properties properties = node->properties;

    for (const auto& label : labels) { index_->remove_label(id, label); }
    for (const auto& [k, v] : properties) { index_->remove_property(id, k, v); }

    metrics_->on_node_deleted(labels, properties);

    store_->remove(id);
    free_ids_->free(id);
  }

} // namespace storage