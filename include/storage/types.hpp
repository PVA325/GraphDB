#pragma once

#include <boost/intrusive_ptr.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

  struct FileHeader {
    uint32_t magic = 0x12345678;
    uint16_t version = 2;
    uint16_t header_size = sizeof(FileHeader);
    uint64_t node_count = 0;
    uint64_t edge_count = 0;
  };

  struct LPVKey {
    uint32_t label_id;
    uint32_t prop_id;
    Value value;
    bool operator==(const LPVKey& item) const {
      return label_id == item.label_id &&
             prop_id == item.prop_id &&
             value == item.value;
    }
  };
  struct LPVKeyHash { size_t operator()(const LPVKey& k) const; };

  struct LPKey {
    uint32_t label_id;
    uint32_t prop_id;
    bool operator==(const LPKey& o) const {
      return label_id == o.label_id && prop_id == o.prop_id;
    }
  };

  struct LPKeyHash {
    size_t operator()(const LPKey& key) const {
      return std::hash<uint64_t>{}((uint64_t)key.label_id << 32 | key.prop_id);
    }
  };

  size_t LPVKeyHash::operator()(const LPVKey& k) const {
    size_t h = std::hash<uint32_t>{}(k.label_id);
    h ^= std::hash<uint32_t>{}(k.prop_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<Value>{}(k.value) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }

  // change name
  struct Delta {
    std::unordered_map<LPVKey, int32_t, LPVKeyHash> count_delta;
    std::unordered_map<LPKey, std::unordered_set<Value>, LPKeyHash> new_distinct;
    size_t write_count = 0;
    bool empty() const { return write_count == 0; }
    void clear() { count_delta.clear(); new_distinct.clear(); write_count = 0; }
  };
} // namespace storage
