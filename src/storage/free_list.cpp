#include "storage/free_list.hpp"

namespace storage {
  size_t FreeList::next(size_t current_size) {
    if (free_ids_.empty()) {
      return current_size;
    }
    size_t id = free_ids_.back();
    free_ids_.pop_back();
    return id;
  }

  void FreeList::free(size_t id) {
    free_ids_.push_back(id);
  }
} // namespace storage