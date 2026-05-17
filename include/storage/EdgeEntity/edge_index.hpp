#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "storage/types.hpp"

namespace storage {
  class EdgeIndex {
  public:
    void add_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type);
    void remove_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type);
    void retype_edge(EdgeId id, const std::string& old_type, const std::string& new_type);

    const std::vector<EdgeId>& outgoing(NodeId src) const;
    const std::vector<EdgeId>& incoming(NodeId dst) const;
    const std::vector<EdgeId>& by_type(const std::string& t) const;

    size_t count_by_type(const std::string& type) const;
    size_t out_degree(NodeId id) const;
    void clear();

  private:
    static void vec_erase(std::vector<EdgeId>& v, EdgeId id);

    static const std::vector<EdgeId> kEmpty;

    std::unordered_map<NodeId, std::vector<EdgeId>> outgoing_;
    std::unordered_map<NodeId, std::vector<EdgeId>> incoming_;
    std::unordered_map<std::string, std::vector<EdgeId>> type_index_;
  };
} // namespace storage