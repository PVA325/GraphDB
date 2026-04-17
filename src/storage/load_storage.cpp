#include <iostream>
#include <fstream>
#include "storage.hpp"

namespace storage {
  bool GraphStorage::load(GraphDB& db, const std::string& path) {
    std::ifstream is(path, std::ios::binary);

    if (!is) {
      return false;
    }

    FileHeader header;
    is.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!is) {
      return false;
    }

    if (header.magic != 0x12345678) {
      return false;
    }
    if (header.version != 1) {
      return false;
    }


    // nodes
    db.nodes_.clear();
    db.free_node_ids.clear();
    db.label_index_.clear();
    db.property_index_.clear();

    db.nodes_.resize(header.node_count);

    for (size_t i = 0; i < db.nodes_.size(); ++i) {
      Node node;

      is.read(reinterpret_cast<char*>(&node.id), sizeof(node.id));
      is.read(reinterpret_cast<char*>(&node.alive), sizeof(node.alive));

      size_t label_count;
      is.read(reinterpret_cast<char*>(&label_count), sizeof(label_count));

      node.labels.resize(label_count);
      if (!is) {
        return false;
      }

      for (size_t j = 0; j < label_count; ++j) {
        size_t len;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!is) {
          return false;
        }

        std::string label;
        label.resize(len);

        is.read(label.data(), len);
        db.label_index_[label].push_back(node.id);
        node.labels[j] = std::move(label);
      }

      node.properties = deserialize_properties(is);
      db.nodes_[i] = std::move(node);

      for (const auto& [key, val] : node.properties) {
        db.property_index_[key][val].push_back(node.id);
      }
    }

    // edges
    db.edges_.clear();
    db.free_edge_ids.clear();

    db.outgoing_.clear();
    db.incoming_.clear();

    db.edges_.resize(header.edge_count);

    for (size_t i = 0; i < db.edges_.size(); ++i) {
      Edge edge;

      is.read(reinterpret_cast<char*>(&edge.id), sizeof(edge.id));
      is.read(reinterpret_cast<char*>(&edge.alive), sizeof(edge.alive));
      is.read(reinterpret_cast<char*>(&edge.src), sizeof(edge.src));
      is.read(reinterpret_cast<char*>(&edge.dst), sizeof(edge.dst));

      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));
      if (!is) {
        return false;
      }

      edge.type.resize(len);
      is.read(edge.type.data(), len);

      if (!is) {
        return false;
      }
      edge.properties = deserialize_properties(is);

      db.outgoing_[edge.src].push_back(edge.id);
      db.incoming_[edge.dst].push_back(edge.id);
      db.edge_type_index_[edge.type].push_back(edge.id);

      db.edges_[i] = std::move(edge);
    }

    //free_ids
    {
      db.free_node_ids.clear();

      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));

      db.free_node_ids.resize(len);
      for (size_t i = 0; i < len; ++i) {
        NodeId id;
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
        db.free_node_ids[i] = id;
      }
    }

    if (!is) {
      return false;
    }

    {
      db.free_edge_ids.clear();

      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));

      db.free_edge_ids.resize(len);
      for (size_t i = 0; i < len; ++i) {
        EdgeId id;
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
        db.free_edge_ids[i] = id;
      }
    }

    return is.good();
  }

  Value GraphStorage::deserialize_value(std::istream& is) {
    uint8_t type;
    is.read(reinterpret_cast<char*>(&type), sizeof(type));

    switch (type) {
      case 0: {
        int64_t value;
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
      }

      case 1: {
        double value;
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
      }

      case 2: {
        std::string str;
        size_t len;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        str.resize(len);

        is.read(str.data(), len);
        return str;
      }

      case 3: {
        bool value;
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
      }

      default:
        throw std::runtime_error("Unknown variant type");
    }
  }

  Properties GraphStorage::deserialize_properties(std::istream& is) {
    size_t count;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));
    Properties properties;

    for (size_t i = 0; i < count; ++i) {
      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));

      std::string key;
      key.resize(len);
      is.read(key.data(), len);

      Value val = deserialize_value(is);
      properties[key] = val;
    }
    return properties;
  }

} // namespace