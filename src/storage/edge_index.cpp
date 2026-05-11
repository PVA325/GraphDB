#include <algorithm>

#include "edge_index.hpp"

namespace storage {

  void EdgeIndex::vec_erase(std::vector<EdgeId>& v, EdgeId id) {
    v.erase(std::remove(v.begin(), v.end(), id), v.end());
  }

  void EdgeIndex::add_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type) {
    auto& out = outgoing_[src];
    if (!out) {out.reset(new EdgeIdList{});}
    out->data.push_back(id);

    auto& in = incoming_[dst];
    if (!in) {in.reset(new EdgeIdList{});}
    in->data.push_back(id);

    auto& by_t = type_index_[type];
    if (!by_t) {by_t.reset(new EdgeIdList{});}
    by_t->data.push_back(id);
  }

  void EdgeIndex::remove_edge(EdgeId id, NodeId src, NodeId dst, const std::string& type) {
    auto oit = outgoing_.find(src);
    if (oit != outgoing_.end()) {
      vec_erase(oit->second->data, id);
    }

    auto iit = incoming_.find(dst);
    if (iit != incoming_.end()) {
      vec_erase(iit->second->data, id);
    }

    auto tit = type_index_.find(type);
    if (tit != type_index_.end()) {vec_erase(tit->second->data, id);}
  }

  void EdgeIndex::retype_edge(EdgeId id,
                              const std::string& old_type,
                              const std::string& new_type) {
    auto old_it = type_index_.find(old_type);
    if (old_it != type_index_.end()) vec_erase(old_it->second->data, id);

    auto& by_t = type_index_[new_type];
    if (!by_t) { by_t.reset(new EdgeIdList{}); }
    by_t->data.push_back(id);
  }

  boost::intrusive_ptr<EdgeIdList> EdgeIndex::outgoing(NodeId src) const {
    auto it = outgoing_.find(src);
    return it != outgoing_.end() ? it->second : nullptr;
  }

  boost::intrusive_ptr<EdgeIdList> EdgeIndex::incoming(NodeId dst) const {
    auto it = incoming_.find(dst);
    return it != incoming_.end() ? it->second : nullptr;
  }

  boost::intrusive_ptr<EdgeIdList> EdgeIndex::by_type(const std::string& t) const {
    auto it = type_index_.find(t);
    return it != type_index_.end() ? it->second : nullptr;
  }

  size_t EdgeIndex::count_by_type(const std::string& type) const {
    auto it = type_index_.find(type);
    return it != type_index_.end() ? it->second->data.size() : 0;
  }

  size_t EdgeIndex::out_degree(NodeId id) const {
    auto it = outgoing_.find(id);
    return it != outgoing_.end() ? it->second->data.size() : 0;
  }

  void EdgeIndex::clear() {
    outgoing_.clear();
    incoming_.clear();
    type_index_.clear();
  }

} // namespace storage