#pragma once

#include <boost/intrusive_ptr.hpp>
#include <string>
#include <unordered_map>

#include "types.hpp"

namespace storage {
  class NodeIndex {
  public:
    void add_label(NodeId id, const std::string& label);
    void remove_label(NodeId id, const std::string& label);

    boost::intrusive_ptr<NodeIdList> by_label(const std::string& label) const;
    size_t count_by_label(const std::string& label) const;

    void add_property(NodeId id, const std::string& key, const Value& val);
    void remove_property(NodeId id, const std::string& key, const Value& val);
    void update_property(NodeId id, const std::string& key,
                         const Value& old_val, const Value& new_val);

    boost::intrusive_ptr<NodeIdList> by_property(const std::string& key,
                                                 const Value& val) const;
    void clear();
  private:
    std::unordered_map<std::string, boost::intrusive_ptr<NodeIdList>> label_index_;
    std::unordered_map<std::string,
      std::unordered_map<Value, boost::intrusive_ptr<NodeIdList>>> property_index_;

    static void vec_erase(std::vector<NodeId>& v, NodeId id);
  };
} // namespace storage