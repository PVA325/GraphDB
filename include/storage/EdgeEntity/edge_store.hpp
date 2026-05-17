#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

#include "storage/page_cache.hpp"
#include "storage/types.hpp"

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
    explicit EdgeStore(const std::string& dir);
    ~EdgeStore();

    void put(const Edge& edge);
    [[nodiscard]] Edge* get(EdgeId id);
    void remove(EdgeId id);
    void flush();

    [[nodiscard]] size_t slot_count() const { return slot_count_; }

  private:
    [[nodiscard]] EdgeSlot read_slot(EdgeId id);
    void write_slot(EdgeId id, const EdgeSlot& slot);
    [[nodiscard]] Edge deserialize(EdgeId id, const EdgeSlot& slot);
    [[nodiscard]] size_t serialize(const Edge& edge);
    void evict_obj_cache();

    std::fstream slots_file_;
    std::fstream props_file_;
    PageCache slots_cache_;
    PageCache props_cache_;

    size_t slot_count_ = 0;
    size_t props_end_ = 0;

    std::unordered_map<EdgeId, Edge> obj_cache_;

  public:
    static constexpr size_t kMaxSlotsPageAmount = 256;
    static constexpr size_t kMaxPropsPageAmount = 512;
    static constexpr size_t kMaxEdgeAmount = 4096;
  };

} // namespace storage