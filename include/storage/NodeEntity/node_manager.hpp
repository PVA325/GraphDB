#pragma once

#include <string>
#include <vector>

#include "storage/EdgeEntity/edge_index.hpp"
#include "storage/free_list.hpp"
#include "storage/metrics_store.hpp"
#include "node_index.hpp"
#include "node_store.hpp"
#include "storage/types.hpp"

namespace storage {

  class NodeManager {
  public:
    NodeManager(NodeStore* store,
                NodeIndex* index,
                EdgeIndex* edge_index,
                MetricsStore* metrics,
                FreeList* free_ids);

    NodeId create(const std::vector<std::string>& labels, const Properties& props);
    Node* get(NodeId id);

    void set_property(NodeId id, const std::string& key, const Value& val);
    void add_label(NodeId id, const std::string& label);
    void remove_label(NodeId id, const std::string& label);
    void remove(NodeId id);

  private:
    NodeStore* store_;
    NodeIndex* index_;
    EdgeIndex* edge_index_;
    MetricsStore* metrics_;
    FreeList* free_ids_;
  };
} // namespace storage