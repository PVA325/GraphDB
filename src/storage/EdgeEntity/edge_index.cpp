#include <algorithm>

#include "storage/EdgeEntity/edge_index.hpp"

namespace storage {

  const std::vector<EdgeId> EdgeIndex::kEmpty{};

  void EdgeIndex::vec_erase(std::vector<EdgeId>& v, EdgeId id) {
    v.erase(std::remove(v.begin(), v.end(), id), v.end());
  }

  void EdgeIndex::add_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type) {
    outgoing_[src].push_back(id);
    incoming_[dst].push_back(id);
    type_index_[type].push_back(id);
  }

  void EdgeIndex::remove_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type) {
    auto oit = outgoing_.find(src);
    if (oit != outgoing_.end()) vec_erase(oit->second, id);

    auto iit = incoming_.find(dst);
    if (iit != incoming_.end()) vec_erase(iit->second, id);

    auto tit = type_index_.find(type);
    if (tit != type_index_.end()) vec_erase(tit->second, id);
  }

  void EdgeIndex::retype_edge(EdgeId id,
                              const std::string& old_type,
                              const std::string& new_type) {
    auto old_it = type_index_.find(old_type);
    if (old_it != type_index_.end()) vec_erase(old_it->second, id);
    type_index_[new_type].push_back(id);
  }

  const std::vector<EdgeId>& EdgeIndex::outgoing(NodeId src) const {
    auto it = outgoing_.find(src);
    return it != outgoing_.end() ? it->second : kEmpty;
  }

  const std::vector<EdgeId>& EdgeIndex::incoming(NodeId dst) const {
    auto it = incoming_.find(dst);
    return it != incoming_.end() ? it->second : kEmpty;
  }

  const std::vector<EdgeId>& EdgeIndex::by_type(const std::string& t) const {
    auto it = type_index_.find(t);
    return it != type_index_.end() ? it->second : kEmpty;
  }

  size_t EdgeIndex::count_by_type(const std::string& type) const {
    auto it = type_index_.find(type);
    return it != type_index_.end() ? it->second.size() : 0;
  }

  size_t EdgeIndex::out_degree(NodeId id) const {
    auto it = outgoing_.find(id);
    return it != outgoing_.end() ? it->second.size() : 0;
  }

  void EdgeIndex::clear() {
    outgoing_.clear();
    incoming_.clear();
    type_index_.clear();
  }

} // namespace storage