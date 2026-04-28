#include <string>
#include <optional>
#include <unordered_set>
#include "storage.hpp"

namespace storage {
  size_t GraphDB::node_count() const {
    return (nodes_.size() - free_node_ids.size());
  }

  size_t GraphDB::node_count_with_label(const std::string& label) const {
    auto it = label_index_.find(label);
    if (it == label_index_.end()) {
      return 0;
    }
    return it->second->data.size();
  }

  size_t GraphDB::edge_count_with_type(const std::string& type) const {
    auto it = edge_type_index_.find(type);
    if (it == edge_type_index_.end()) return 0;
    return it->second->data.size();
  }



  double GraphDB::avg_out_degree(const std::string& label) const {
    auto lit = label_index_.find(label);
    if (lit == label_index_.end()) {
      return 0.0;
    }
    size_t count = lit->second->data.size();
    if (count == 0) {
      return 0.0;
    }
    return static_cast<double>(label_total_out_degree_.at(label)) / count;
  }

  bool GraphDB::has_property_index(const std::string& label,
                                   const std::string& property) const {
    auto lit = label_property_count_.find(label);
    if (lit == label_property_count_.end()) return false;
    auto pit = lit->second.find(property);
    if (pit == lit->second.end()) return false;
    return !pit->second.empty();
  }

  std::optional<size_t> GraphDB::property_distinct_count(
    const std::string& property,
    const std::string& label) const {

    if (label.empty()) {
      auto it = property_distinct_.find(property);
      if (it == property_distinct_.end()) return 0;
      return it->second.size();
    }

    auto lit = label_property_distinct_.find(label);
    if (lit == label_property_distinct_.end()) return 0;
    auto pit = lit->second.find(property);
    if (pit == lit->second.end()) return 0;
    return pit->second.size();
  }

  size_t GraphDB::property_count(
    const std::string& property,
    const Value& value,
    const std::string& label) const {

    if (label.empty()) {
      auto pit = property_index_.find(property);
      if (pit == property_index_.end()) return 0;
      auto vit = pit->second.find(value);
      if (vit == pit->second.end()) return 0;
      return vit->second->data.size();
    }

    auto lit = label_property_count_.find(label);
    if (lit == label_property_count_.end()) return 0;
    auto pit = lit->second.find(property);
    if (pit == lit->second.end()) return 0;
    auto vit = pit->second.find(value);
    if (vit == pit->second.end()) return 0;
    return vit->second;
  }
} // namespace