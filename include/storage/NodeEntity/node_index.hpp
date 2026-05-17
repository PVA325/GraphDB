#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "storage/types.hpp"

namespace storage {
  class NodeIndex {
  public:
    void add_label(NodeId id, const std::string& label);
    void remove_label(NodeId id, const std::string& label);

    const std::vector<NodeId>& by_label(const std::string& label) const;
    size_t count_by_label(const std::string& label) const;

    void add_property(NodeId id, const std::string& key, const Value& val);
    void remove_property(NodeId id, const std::string& key, const Value& val);
    void update_property(NodeId id, const std::string& key,
                         const Value& old_val, const Value& new_val);

    const std::vector<NodeId>& by_property(const std::string& key,
                                           const Value& val) const;
    void clear();

  private:
    static const std::vector<NodeId> kEmpty;

    std::unordered_map<std::string, std::vector<NodeId>> label_index_;
    std::unordered_map<std::string,
      std::unordered_map<Value, std::vector<NodeId>>> property_index_;

    static void vec_erase(std::vector<NodeId>& v, NodeId id);
  };
} // namespace storage