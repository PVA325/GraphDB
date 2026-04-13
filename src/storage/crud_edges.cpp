#include <algorithm>
#include <cassert>
#include "storage.hpp"

namespace storage {

  EdgeId GraphDB::create_edge(NodeId src, NodeId dst,
                              const std::string& type,
                              const Properties& props) {
    EdgeId id;

    if (!free_edge_ids.empty()) {
      id = free_edge_ids.top();
      free_edge_ids.pop();

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

    // adjacency maps
    outgoing_[src].push_back(id);
    incoming_[dst].push_back(id);

    // type index
    edge_type_index_[type].push_back(id);

    return id;
  }


  Edge* GraphDB::get_edge(EdgeId id) {
    if (id >= edges_.size() || !edges_[id].alive) {
      return nullptr;    // возвращаем nullptr, если узел удалён
    }

    return &edges_[id];
  }


  void GraphDB::set_edge_property(EdgeId id, const std::string& key, const Value& val) {
    assert(id < edges_.size());
    assert(edges_[id].alive);

    Edge& edge = edges_[id];

    auto it = edge.properties.find(key);
    if (it != edge.properties.end()) {
      const Value& old_val = it->second;
      // edge properties index not implemented yet
      // if you make index later, remove old value from it
    }

    edge.properties[key] = val;
  }


  void GraphDB::delete_edge(EdgeId id) {
    assert(id < edges_.size());
    assert(edges_[id].alive);

    Edge& edge = edges_[id];

    // remove from adjacency
    auto& out_vec = outgoing_[edge.src];
    out_vec.erase(std::remove(out_vec.begin(), out_vec.end(), id), out_vec.end());

    auto& in_vec = incoming_[edge.dst];
    in_vec.erase(std::remove(in_vec.begin(), in_vec.end(), id), in_vec.end());

    // remove from type index
    auto& type_vec = edge_type_index_[edge.type];
    type_vec.erase(std::remove(type_vec.begin(), type_vec.end(), id), type_vec.end());

    // mark as deleted
    edge.properties.clear();
    edge.type.clear();
    edges_[id].alive = false;

    free_edge_ids.push(id);
  }

}