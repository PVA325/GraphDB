#include <cassert>
#include "storage.hpp"

namespace storage {
  NodeId GraphDB::create_node(const std::vector<std::string>& labels,
                              const Properties& props) {

    NodeId id;
    if (free_node_ids.empty()) {
      id = nodes_.size();

      nodes_.push_back(Node{
        .id = id,
        .alive = true,
        .labels = labels,
        .properties = props
      });
    } else {
      id = free_node_ids.top();
      free_node_ids.pop();

      nodes_[id] = Node{
        .id = id,
        .alive = true,
        .labels = labels,
        .properties = props
      };
    }

    // refresh label_index
    for (const auto& label : labels) {
      label_index_[label].push_back(id);
    }

    // refresh property_index
    for (const auto& [key, value] : props) {
      property_index_[key][value].push_back(id);
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
      auto& vec = property_index_[key][old_val];
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    node.properties[key] = val;
    property_index_[key][val].push_back(id);
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

    // delete from label_index
    for (const auto& label : node.labels) {
      auto& vec = label_index_[label];
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }

    // delete from property_index
    for (const auto& [key, value] : node.properties) {
      auto& vec = property_index_[key][value];
      vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    }


    // delete incedent edges
    if (outgoing_.count(id)) {
      auto edges = outgoing_[id];
      for (auto eid : edges) {
        delete_edge(eid);
      }
      outgoing_.erase(id);
    }

    if (incoming_.count(id)) {
      auto edges = incoming_[id];
      for (auto eid : edges) {
        delete_edge(eid);
      }
      incoming_.erase(id);
    }

    node.labels.clear();
    node.properties.clear();
    nodes_[id].alive = false;
    free_node_ids.push(id);
  }
}