#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "types.hpp"

namespace storage {
  class GraphDB;

  class GraphStorage {
  public:
    static bool save(const GraphDB& db, const std::string& path);
    static bool load(GraphDB& db, const std::string& path);

  private:
    struct FileHeader {
      uint32_t magic = 0x12345678;
      uint16_t version = 2;
      uint16_t header_size = sizeof(FileHeader);
      uint64_t node_count = 0;
      uint64_t edge_count = 0;
    };

    static void serialize_value(std::ostream& os, const Value& val);
    static Value deserialize_value(std::istream& is);
    static void serialize_properties(std::ostream& os, const Properties& p);
    static Properties deserialize_properties(std::istream& is);
  };

} // namespace storage