#include <stdexcept>

#include "storage/string_interner.hpp"

namespace storage {

  uint32_t StringInterner::intern(const std::string& s) {
    auto it = str_to_id_.find(s);
    if (it != str_to_id_.end()) {
      return it->second;
    }
    uint32_t id = static_cast<uint32_t>(id_to_str_.size());
    str_to_id_[s] = id;
    id_to_str_.push_back(s);
    return id;
  }

  uint32_t StringInterner::find(const std::string& s) const {
    auto it = str_to_id_.find(s);
    return it != str_to_id_.end() ? it->second : kUnvalidID;
  }

  const std::string& StringInterner::get(uint32_t id) const {
    if (id >= id_to_str_.size()) {
      throw std::out_of_range("StringInterner: invalid id");
    }
    return id_to_str_[id];
  }
} // namespace storage