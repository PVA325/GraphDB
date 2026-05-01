#include <cassert>
#include "storage/storage.hpp"

namespace storage {
  NodeId GraphDB::create_node(const std::vector<std::string>& labels,
                              const Properties& props) {

    NodeId id;
    if (free_node_ids.empty()) {
      id = nodes_.size();

      nodes_.emplace_back(Node{
        .id = id,
        .alive = true,
        .labels = labels,
        .properties = props
      });
    } else {
      id = free_node_ids.back();
      free_node_ids.pop_back();

      nodes_[id] = Node{
        .id = id,
        .alive = true,
        .labels = labels,
        .properties = props
      };
    }

    for (const auto& label : labels) {
      label_index_.try_emplace(label, new NodeIdList{});
      label_index_[label]->data.emplace_back(id);
    }

    for (const auto& [key, value] : props) {
      property_index_[key].try_emplace(value, new NodeIdList{});
      property_index_[key][value]->data.emplace_back(id);
    }

    for (const auto& label : labels) {
      for (const auto& [key, val] : props) {
        label_property_distinct_[label][key].insert(val);
        label_property_count_[label][key][val]++;
      }
    }
    for (const auto& [key, val] : props) {
      property_distinct_[key].insert(val);
    }

    return id;
  }

  void GraphDB::set_node_property(storage::NodeId id, const std::string &key, const storage::Value &val) {
    assert(id < nodes_.size());
    assert(nodes_[id].alive);

    Node& node = nodes_[id];

    auto it = node.properties.find(key);
    if (it != node.properties.end()) {
      const Value& old_val = it->second;
      auto& vec = property_index_[key][old_val]->data;
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    node.properties[key] = val;
    property_index_[key].try_emplace(val, new NodeIdList{});
    property_index_[key][val]->data.emplace_back(id);
  }

  void GraphDB::set_node_label(NodeId id, const std::string& label) {
    assert(id < nodes_.size());
    assert(nodes_[id].alive);

    Node& node = nodes_[id];

    auto it = std::find(node.labels.begin(), node.labels.end(), label);
    if (it == node.labels.end()) {
      label_index_.try_emplace(label, new NodeIdList{});
      label_index_[label]->data.emplace_back(id);
      node.labels.emplace_back(label);

      auto oit = outgoing_.find(id);
      if (oit != outgoing_.end()) {
        label_total_out_degree_[label] += oit->second->data.size();
      }
    }
  }

  Node* GraphDB::get_node(NodeId id) {
    if (id >= nodes_.size() || !nodes_[id].alive) {
      return nullptr;    // возвращаем nullptr, если узел удалён
    }

    return &nodes_[id];
  }

  void GraphDB::delete_node(NodeId id) {
    assert(id < nodes_.size());
    assert(nodes_[id].alive);

    Node& node = nodes_[id];

    for (const auto& label : node.labels) {
      auto& vec = label_index_[label]->data;
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    for (const auto& [key, value] : node.properties) {
      auto& vec = property_index_[key][value]->data;
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    for (const auto& label : node.labels) {
      auto oit = outgoing_.find(id);
      if (oit != outgoing_.end()) {
        label_total_out_degree_[label] -= oit->second->data.size();
      }
    }
    if (outgoing_.count(id)) {
      auto edges = outgoing_[id]->data;
      for (auto eid : edges) {
        delete_edge(eid);
      }
      outgoing_.erase(id);
    }

    if (incoming_.count(id)) {
      auto edges = incoming_[id]->data;
      for (auto eid : edges) {
        delete_edge(eid);
      }
      incoming_.erase(id);
    }

    node.labels.clear();
    node.properties.clear();
    nodes_[id].alive = false;
    free_node_ids.emplace_back(id);
  }

  void GraphDB::delete_node_label(NodeId id, const std::string& label) {
    auto& vec = label_index_[label]->data;
    vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());

    auto oit = outgoing_.find(id);
    if (oit != outgoing_.end()) {
      label_total_out_degree_[label] -= oit->second->data.size();
    }
    auto& labels = nodes_[id].labels;
    labels.erase(std::remove(labels.begin(), labels.end(), label), labels.end());
  }
}