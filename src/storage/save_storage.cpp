  #include <iostream>
#include <fstream>
#include "storage.hpp"

namespace storage {
  bool GraphStorage::save(const GraphDB& db, const std::string& path) {
    std::ofstream os(path, std::ios::binary);
    if (!os) {
      return false;
    }

    FileHeader header{
      .node_count = db.nodes_.size(),
      .edge_count = db.edges_.size()
    };

    os.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // nodes
    for (const auto& node: db.nodes_) {
      os.write(reinterpret_cast<const char*>(&node.id), sizeof(node.id));
      os.write(reinterpret_cast<const char*>(&node.alive), sizeof(node.alive));

      size_t label_count = node.labels.size();
      os.write(reinterpret_cast<const char*>(&label_count), sizeof(label_count));
      for (const auto& label: node.labels) {
        size_t len = label.size();
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        os.write(label.data(), len);
      }

      serialize_properties(os, node.properties);
    }

    // edges
    for (const auto& edge: db.edges_) {
      os.write(reinterpret_cast<const char*>(&edge.id), sizeof(edge.id));
      os.write(reinterpret_cast<const char*>(&edge.alive), sizeof(edge.alive));
      os.write(reinterpret_cast<const char*>(&edge.src), sizeof(edge.src));
      os.write(reinterpret_cast<const char*>(&edge.dst), sizeof(edge.dst));

      size_t len = edge.type.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      os.write(edge.type.data(), len);

      serialize_properties(os, edge.properties);
    }

    //free_ids
    {
      size_t len = db.free_node_ids.size();
      os.write(reinterpret_cast<const char *>(&len), sizeof(len));
      for (const auto &id: db.free_node_ids) {
        os.write(reinterpret_cast<const char *>(&id), sizeof(id));
      }
    }

    {
      size_t len = db.free_edge_ids.size();
      os.write(reinterpret_cast<const char *>(&len), sizeof(len));
      for (const auto &id: db.free_edge_ids) {
        os.write(reinterpret_cast<const char *>(&id), sizeof(id));
      }
    }

    return os.good();
  }

  void GraphStorage::serialize_properties(std::ostream& os, const Properties& props) {
    size_t count = props.size();
    os.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& [key, val]: props) {
      size_t len = key.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      os.write(key.data(), len);

        serialize_value(os, val);
    }
  }

  void GraphStorage::serialize_value(std::ostream& os, const Value& val) {
    uint8_t type = static_cast<uint8_t>(val.index());
    os.write(reinterpret_cast<const char*>(&type), sizeof(type));

    switch (val.index()) {
      case 0: {
        int64_t value = std::get<0>(val);
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        break;
      }

      case 1: {
        double value = std::get<1>(val);
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        break;
      }

      case 2: {
        const std::string& str = std::get<2>(val);
        size_t len = str.size();
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        os.write(str.data(), len);
        break;
      }

      case 3: {
        bool value = std::get<3>(val);
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        break;
      }

      default:
        throw std::runtime_error("Unknown variant type");
    }
  }
} // namespace