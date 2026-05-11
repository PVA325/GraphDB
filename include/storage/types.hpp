#pragma once

#include <boost/intrusive_ptr.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace storage {

  using NodeId = size_t;
  using EdgeId = size_t;

  using Value = std::variant<int64_t, double, std::string, bool>;
  using Properties = std::unordered_map<std::string, Value>;

  template<typename T>
  struct RefCountedVector {
    std::vector<T> data;
    mutable int ref_count = 0;

    friend void intrusive_ptr_add_ref(const RefCountedVector* p) { ++p->ref_count; }
    friend void intrusive_ptr_release(const RefCountedVector* p) {
      if (--p->ref_count == 0) { delete p; }
    }
  };

  using NodeIdList = RefCountedVector<NodeId>;
  using EdgeIdList = RefCountedVector<EdgeId>;

  struct Node {
    NodeId id;
    bool   alive;
    std::vector<std::string> labels;
    Properties properties;
  };

  struct Edge {
    EdgeId id;
    bool alive;
    NodeId src;
    NodeId dst;
    std::string type;
    Properties properties;
  };

} // namespace storage

/*
namespace std {
  template<>
  struct hash<storage::Value> {
    size_t operator()(const storage::Value& v) const noexcept {
      size_t type_hash = std::hash<size_t>{}(v.index());
      size_t val_hash  = std::visit([](const auto& x) {
        return std::hash<std::decay_t<decltype(x)>>{}(x);
      }, v);
      return type_hash ^ (val_hash + 0x9e3779b9 + (type_hash << 6) + (type_hash >> 2));
    }
  };

} // namespace std
 */