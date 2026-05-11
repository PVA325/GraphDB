#include <algorithm>

#include "storage/node_index.hpp"

namespace storage {
  void NodeIndex::vec_erase(std::vector<NodeId>& v, NodeId id) {
    v.erase(std::remove(v.begin(), v.end(), id), v.end());
  }

  void NodeIndex::add_label(NodeId id, const std::string& label) {
    auto& list = label_index_[label];
    if (!list) { list.reset(new NodeIdList{}); }
    list->data.push_back(id);
  }

  void NodeIndex::remove_label(NodeId id, const std::string& label) {
    auto it = label_index_.find(label);
    if (it == label_index_.end()) {
      return;
    }
    vec_erase(it->second->data, id);
  }

  boost::intrusive_ptr<NodeIdList>
  NodeIndex::by_label(const std::string& label) const {
    auto it = label_index_.find(label);
    return it != label_index_.end() ? it->second : nullptr;
  }

  size_t NodeIndex::count_by_label(const std::string& label) const {
    auto it = label_index_.find(label);
    return it != label_index_.end() ? it->second->data.size() : 0;
  }

  void NodeIndex::add_property(NodeId id, const std::string& key, const Value& val) {
    auto& inner = property_index_[key];
    auto& list  = inner[val];
    if (!list) {
      list.reset(new NodeIdList{});
    }
    list->data.push_back(id);
  }

  void NodeIndex::remove_property(NodeId id, const std::string& key, const Value& val) {
    auto kit = property_index_.find(key);
    if (kit == property_index_.end()) {
      return;
    }
    auto vit = kit->second.find(val);
    if (vit == kit->second.end()) {
      return;
    }
    vec_erase(vit->second->data, id);
  }

  void NodeIndex::update_property(NodeId id, const std::string& key,
                                  const Value& old_val, const Value& new_val) {
    remove_property(id, key, old_val);
    add_property(id, key, new_val);
  }

  boost::intrusive_ptr<NodeIdList>
  NodeIndex::by_property(const std::string& key, const Value& val) const {
    auto kit = property_index_.find(key);
    if (kit == property_index_.end()) {
      return nullptr;
    }
    auto vit = kit->second.find(val);
    return vit != kit->second.end() ? vit->second : nullptr;
  }

  void NodeIndex::clear() {
    label_index_.clear();
    property_index_.clear();
  }
} // namespace storage