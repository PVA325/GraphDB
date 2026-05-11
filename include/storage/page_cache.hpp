#pragma once

#include <fstream>
#include <list>
#include <unordered_map>
#include <vector>

namespace storage {
  class PageCache {
  public:
    static constexpr size_t KPageSize = 4096;

    explicit PageCache(std::fstream& file, size_t max_pages);
    ~PageCache();

    void read(size_t offset, void* dst, size_t size);
    void write(size_t offset, const void* src, size_t size);
    void flush();
    void invalidate();

  private:
    struct Page {
      size_t page_id;
      std::vector<char> data;
      bool dirty = false;
    };

    using LruList = std::list<size_t>;
    using LruIt = LruList::iterator;
    using PageMap = std::unordered_map<size_t, std::pair<Page, LruIt>>;

    std::fstream& file_;
    size_t max_pages_;
    PageMap pages_;
    LruList lru_;

    Page& get_page(size_t page_id);
    void load_page(size_t page_id);
    void evict_lru();
    void flush_page(Page& p);
  };

} // namespace storage