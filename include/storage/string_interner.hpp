#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace storage {
  class StringInterner {
  public:
    static constexpr uint32_t kUnvalidID = UINT32_MAX;

    uint32_t intern(const std::string& s);
    uint32_t find(const std::string& s) const;
    const std::string& get(uint32_t id) const;
    size_t size() const { return id_to_str_.size(); }

  private:
    std::unordered_map<std::string, uint32_t> str_to_id_;
    std::vector<std::string> id_to_str_;
  };
} // namespace storage