#pragma once

#include <boost/intrusive_ptr.hpp>
#include <string>
#include <unordered_map>

#include "types.hpp"

namespace storage {
  class EdgeIndex {
  public:
    void add_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type);
    void remove_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type);

    void retype_edge(EdgeId id, const std::string& old_type, const std::string& new_type);

    boost::intrusive_ptr<EdgeIdList> outgoing(NodeId src) const;
    boost::intrusive_ptr<EdgeIdList> incoming(NodeId dst) const;
    boost::intrusive_ptr<EdgeIdList> by_type(const std::string& t) const;

    size_t count_by_type(const std::string& type) const;
    size_t out_degree(NodeId id) const;
    void clear();
  private:
    std::unordered_map<NodeId, boost::intrusive_ptr<EdgeIdList>> outgoing_;
    std::unordered_map<NodeId, boost::intrusive_ptr<EdgeIdList>> incoming_;
    std::unordered_map<std::string, boost::intrusive_ptr<EdgeIdList>> type_index_;

    static void vec_erase(std::vector<EdgeId>& v, EdgeId id);
  };
} // namespace storage