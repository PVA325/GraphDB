#include <algorithm>
#include <cassert>
#include "storage.hpp"

namespace storage {

  EdgeId GraphDB::create_edge(NodeId src, NodeId dst,
                              const std::string& type,
                              const Properties& props) {
    EdgeId id;

    if (!free_edge_ids.empty()) {
      id = free_edge_ids.back();
      free_edge_ids.pop_back();

      edges_[id] = Edge{
        .id = id,
        .alive = true,
        .src = src,
        .dst = dst,
        .type = type,
        .properties = props
      };
    } else {
      id = edges_.size();
      edges_.push_back(Edge{
        .id = id,
        .alive = true,
        .src = src,
        .dst = dst,
        .type = type,
        .properties = props
      });
    }

    outgoing_[src].push_back(id);
    for (const auto& label : nodes_[src].labels) {
      label_total_out_degree_[label]++;
    }
    incoming_[dst].push_back(id);

    edge_type_index_[type].push_back(id);

    return id;
  }


  Edge* GraphDB::get_edge(EdgeId id) {
    if (id >= edges_.size() || !edges_[id].alive) {
      return nullptr;
    }

    return &edges_[id];
  }


  void GraphDB::set_edge_property(EdgeId id, const std::string& key, const Value& val) {
    assert(id < edges_.size());
    assert(edges_[id].alive);

    Edge& edge = edges_[id];
    edge.properties[key] = val;
  }


  void GraphDB::delete_edge(EdgeId id) {
    assert(id < edges_.size());
    assert(edges_[id].alive);

    Edge& edge = edges_[id];

    auto& out_vec = outgoing_[edge.src];
    out_vec.erase(std::remove(out_vec.begin(), out_vec.end(), id), out_vec.end());

    auto& in_vec = incoming_[edge.dst];
    in_vec.erase(std::remove(in_vec.begin(), in_vec.end(), id), in_vec.end());

    auto& type_vec = edge_type_index_[edge.type];
    type_vec.erase(std::remove(type_vec.begin(), type_vec.end(), id), type_vec.end());

    edge.properties.clear();
    edge.type.clear();
    for (const auto& label : nodes_[edge.src].labels) {
      label_total_out_degree_[label]--;
    }
    edges_[id].alive = false;

    free_edge_ids.push_back(id);
  }

  void GraphDB::set_edge_type(EdgeId id, const std::string& type) {
    Edge& edge = edges_[id];

    auto it = edge_type_index_.find(edge.type);
    if (it != edge_type_index_.end()) {
      it->second.erase(std::remove(it->second.begin(), it->second.end(), id), it->second.end());
    }

    edge.type = type;
    edge_type_index_[type].emplace_back(id);
  }


}