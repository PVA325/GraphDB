#pragma once

#include <cstddef>
#include <vector>

namespace storage {
  class FreeList {
  public:
    size_t next(size_t current_size);
    void free(size_t id);

    [[nodiscard]] size_t free_count() const { return free_ids_.size(); }
    [[nodiscard]] const std::vector<size_t>& free_ids() const { return free_ids_; }

  private:
    std::vector<size_t> free_ids_;
  };
} // namespace storage