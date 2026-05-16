#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

#include "storage/page_cache.hpp"
#include "storage/types.hpp"

namespace storage {
  struct NodeSlot {
    static constexpr size_t kSize = 13;

    uint64_t props_offset = 0;
    uint32_t props_size = 0;
    bool alive = false;
  };

  class NodeStore {
  public:
    static constexpr size_t kMaxSlotsPageAmount = 256;
    static constexpr size_t kMaxPropsPageAmount = 1024;
    static constexpr size_t kMaxNodeAmount = 4096;

    explicit NodeStore(const std::string& dir);
    ~NodeStore();

    void put(NodeId id, const Node& node);
    Node* get(NodeId id);
    void remove(NodeId id);
    void flush();

    size_t slot_count() const { return slot_count_; }

  private:
    std::fstream slots_file_;
    std::fstream props_file_;
    PageCache slots_cache_;
    PageCache props_cache_;

    size_t slot_count_ = 0;
    size_t props_end_ = 0;

    std::unordered_map<NodeId, Node> obj_cache_;

    NodeSlot read_slot(NodeId id);
    void write_slot(NodeId id, const NodeSlot& slot);
    Node deserialise(NodeId id, const NodeSlot& slot);
    size_t serialise(const Node& node);
    void evict_obj_cache();
  };

} // namespace storage