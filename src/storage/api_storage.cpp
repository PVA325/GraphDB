#include <string>
#include <optional>
#include <unordered_set>
#include "storage.hpp"

namespace storage {
  size_t GraphDB::node_count() const {
    return (nodes_.size() - free_node_ids.size());
  }

  size_t GraphDB::node_count_with_label(const std::string& label) const {
    return label_index_.at(label).size();
  }

  std::optional<size_t> GraphDB::property_distinct_count(
      const std::string& label,
      const std::string& property) const{
    auto it = label_index_.find(label);
    if (it == label_index_.end()) {
      return std::nullopt;
    }

    std::unordered_set<Value> distinct;
    for (auto id: it->second) {
      const auto& node = nodes_[id];

      auto pit = node.properties.find(property);
      if (pit != node.properties.end()) {
        distinct.insert(pit->second);
      }
    }

    return distinct.size();
  }

  double GraphDB::avg_out_degree(const std::string& label) const {
    auto it = label_index_.find(label);
    if (it == label_index_.end()) {
      return 0.0;
    }

    size_t total = 0;
    size_t count = 0;
    for (auto id: it->second) {
      auto oit = outgoing_.find(id);
      if (oit != outgoing_.end()) {
        total += oit->second.size();
      }
      ++count;
    }

    return count? static_cast<double>(total) / count : 0.0;
  }

  bool GraphDB::has_property_index(const std::string& label,
                          const std::string& property) const {
    auto it = label_index_.find(label);
    if (it == label_index_.end()) {
      return false;
    }

    if (property_index_.find(property) == property_index_.end()) {
      return false;
    }

    for (auto id: it->second) {
      const auto& node = nodes_[id];
      if (node.properties.count(property)) {
        return true;
      }
    }
    return false;
  }
} // namespace