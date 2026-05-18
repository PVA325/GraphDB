#pragma once

#include <cstdint>
#include <string>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace storage {

  using NodeId = size_t;
  using EdgeId = size_t;

  using Value = std::variant<int64_t, double, std::string, bool>;
  using Properties = std::unordered_map<std::string, Value>;

  struct Node {
    NodeId id;
    bool alive;
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


  struct DeltaEvent {
    enum class Type {
      NodeCreated,
      NodeDeleted,
      LabelAdded,
      LabelRemoved,
      EdgeCreated,
      EdgeDeleted
    };

    Type type;
    std::vector<std::string> labels;
    Properties props;
    size_t out_degree = 0;
  };

  struct Page {
    size_t page_id;
    std::vector<char> data;
    bool dirty = false;
  };

  using LruList = std::list<size_t>;
  using LruIt = LruList::iterator;
  using PageMap = std::unordered_map<size_t, std::pair<Page, LruIt>>;
} // namespace storage