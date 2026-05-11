#pragma once

#include <string>
#include <vector>

#include "edge_index.hpp"
#include "edge_store.hpp"
#include "free_list.hpp"
#include "metrics_store.hpp"
#include "node_store.hpp"
#include "types.hpp"

namespace storage {
  class EdgeManager {
  public:
    EdgeManager(EdgeStore* store,
                EdgeIndex* index,
                NodeStore* node_store,
                MetricsStore* metrics,
                FreeList* free_ids);

    EdgeId create(NodeId src, NodeId dst,
                  const std::string& type,
                  const Properties& props);

    Edge* get(EdgeId id);

    void set_property(EdgeId id, const std::string& key, const Value& val);
    void set_type(EdgeId id, const std::string& new_type);
    void remove(EdgeId id);

  private:
    EdgeStore* store_;
    EdgeIndex* index_;
    NodeStore* node_store_;
    MetricsStore* metrics_;
    FreeList* free_ids_;
  };
} // namespace storage