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
    static void serialize_value(std::ostream& os, const Value& val);
    [[nodiscard]] static Value deserialize_value(std::istream& is);
    static void serialize_properties(std::ostream& os, const Properties& p);
    [[nodiscard]] static Properties deserialize_properties(std::istream& is);
  };

} // namespace storage