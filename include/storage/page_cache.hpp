#pragma once

#include <fstream>
#include <list>
#include <unordered_map>
#include <vector>

#include "storage/TypesStructures/types.hpp"

namespace storage {

  class PageCache {
  public:
    explicit PageCache(std::fstream& file, size_t max_pages);
    ~PageCache();

    void read(size_t offset, void* dst, size_t size);
    void write(size_t offset, const void* src, size_t size);
    void flush();
    void invalidate();

  private:
    Page& get_page(size_t page_id);
    void load_page(size_t page_id);
    void evict_lru();
    void flush_page(Page& p);

    std::fstream& file_;
    size_t max_pages_;
    PageMap pages_;
    LruList lru_;

  public:
    static constexpr size_t KPageSize = 4096;
  };

} // namespace storage