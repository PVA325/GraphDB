#include <cassert>
#include <cstring>
#include <stdexcept>

#include "storage/page_cache.hpp"

namespace storage {
  PageCache::PageCache(std::fstream& file, size_t max_pages)
    : file_(file), max_pages_(max_pages) {}

  PageCache::~PageCache() {
    flush();
  }

  void PageCache::read(size_t offset, void* dst, size_t size) {
    size_t bytes_read = 0;
    while (bytes_read < size) {
      size_t cur_offset = offset + bytes_read;
      size_t page_id = cur_offset / KPageSize;
      size_t page_off = cur_offset % KPageSize;
      size_t chunk = std::min(size - bytes_read, KPageSize - page_off);

      Page& page = get_page(page_id);
      std::memcpy(static_cast<char*>(dst) + bytes_read,
                  page.data.data() + page_off, chunk);
      bytes_read += chunk;
    }
  }

  void PageCache::write(size_t offset, const void* src, size_t size) {
    size_t bytes_written = 0;
    while (bytes_written < size) {
      size_t cur_offset = offset + bytes_written;
      size_t page_id = cur_offset / KPageSize;
      size_t page_off = cur_offset % KPageSize;
      size_t chunk = std::min(size - bytes_written, KPageSize - page_off);

      Page& page = get_page(page_id);
      std::memcpy(page.data.data() + page_off,
                  static_cast<const char*>(src) + bytes_written, chunk);
      page.dirty = true;
      bytes_written += chunk;
    }
  }

  void PageCache::flush() {
    for (auto& [pid, entry] : pages_) {
      flush_page(entry.first);
    }
  }

  void PageCache::invalidate() {
    flush();
    pages_.clear();
    lru_.clear();
  }

  PageCache::Page& PageCache::get_page(size_t page_id) {
    auto it = pages_.find(page_id);
    if (it != pages_.end()) {
      lru_.erase(it->second.second);
      lru_.push_back(page_id);
      it->second.second = std::prev(lru_.end());
      return it->second.first;
    }

    if (pages_.size() >= max_pages_) {
      evict_lru();
    }

    load_page(page_id);
    return pages_.at(page_id).first;
  }

  void PageCache::load_page(size_t page_id) {
    Page page;
    page.page_id = page_id;
    page.data.resize(KPageSize, 0);
    page.dirty = false;

    size_t file_offset = page_id * KPageSize;
    file_.seekg(static_cast<std::streamoff>(file_offset), std::ios::beg);
    if (file_) {
      file_.read(page.data.data(), KPageSize);
    }

    lru_.push_back(page_id);
    LruIt it = std::prev(lru_.end());
    pages_.emplace(page_id, std::make_pair(std::move(page), it));
  }

  void PageCache::evict_lru() {
    assert(!lru_.empty());
    size_t victim_id = lru_.front();
    lru_.pop_front();

    auto it = pages_.find(victim_id);
    assert(it != pages_.end());
    flush_page(it->second.first);
    pages_.erase(it);
  }

  void PageCache::flush_page(Page& page) {
    if (!page.dirty) {
      return;
    }

    size_t file_offset = page.page_id * KPageSize;
    file_.seekp(static_cast<std::streamoff>(file_offset), std::ios::beg);
    file_.write(page.data.data(), KPageSize);
    file_.flush();
    page.dirty = false;
  }
} // namespace storage