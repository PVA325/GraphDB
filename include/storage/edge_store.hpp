#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

#include "page_cache.hpp"
#include "types.hpp"

namespace storage {
  struct EdgeSlot {
    static constexpr size_t kSize = 29;

    uint64_t props_offset = 0;
    NodeId src = 0;
    NodeId dst = 0;
    uint32_t props_size = 0;
    bool alive = false;
  };

  class EdgeStore {
  public:
    static constexpr size_t kMaxSlotsPageAmount = 256;
    static constexpr size_t kMaxPropsPageAmount = 512;
    static constexpr size_t kMaxNodeAmount = 4096;

    explicit EdgeStore(const std::string& dir);
    ~EdgeStore();

    void put(EdgeId id, const Edge& edge);
    Edge* get(EdgeId id);
    void remove(EdgeId id);
    void flush();

    size_t slot_count() const { return slot_count_; }

  private:
    std::fstream slots_file_;
    std::fstream props_file_;
    PageCache slots_cache_;
    PageCache props_cache_;

    size_t slot_count_ = 0;
    size_t props_end_ = 0;

    std::unordered_map<EdgeId, Edge> obj_cache_;

    EdgeSlot read_slot(EdgeId id);
    void write_slot(EdgeId id, const EdgeSlot& slot);
    Edge deserialise(EdgeId id, const EdgeSlot& slot);
    size_t serialise(const Edge& edge);
    void evict_obj_cache();
  };

} // namespace storage