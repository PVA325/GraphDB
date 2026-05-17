#include <stdexcept>

#include "storage/graph_storage.hpp"
#include "storage/graph_db.hpp"

namespace storage {
  bool GraphStorage::save(const GraphDB& db, const std::string& path) {
    std::ofstream os(path, std::ios::binary);
    if (!os) { return false; }

    bool dm = db.disk_mode();

    size_t node_count = dm ? db.node_store_->slot_count() : db.nodes_ram_.size();
    size_t edge_count = dm ? db.edge_store_->slot_count() : db.edges_ram_.size();

    FileHeader header;
    header.node_count = node_count;
    header.edge_count = edge_count;
    os.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (size_t i = 0; i < node_count; ++i) {
      Node* node = dm ? db.node_store_->get(i)
                      : (i < db.nodes_ram_.size() ? const_cast<Node*>(&db.nodes_ram_[i]) : nullptr);

      bool alive = node && node->alive;
      NodeId nid = i;
      os.write(reinterpret_cast<const char*>(&nid),sizeof(nid));
      os.write(reinterpret_cast<const char*>(&alive),  sizeof(alive));

      size_t label_count = alive ? node->labels.size() : 0;
      os.write(reinterpret_cast<const char*>(&label_count), sizeof(label_count));

      if (alive) {
        for (const auto& label : node->labels) {
          size_t len = label.size();
          os.write(reinterpret_cast<const char*>(&len), sizeof(len));
          os.write(label.data(), len);
        }
        serialize_properties(os, node->properties);
      } else {
        size_t zero = 0;
        os.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
      }
    }

    for (size_t i = 0; i < edge_count; ++i) {
      Edge* edge = dm ? db.edge_store_->get(i)
                      : (i < db.edges_ram_.size() ? const_cast<Edge*>(&db.edges_ram_[i]) : nullptr);

      bool alive = edge && edge->alive;
      EdgeId eid  = i;
      NodeId src  = alive ? edge->src : 0;
      NodeId dst  = alive ? edge->dst : 0;

      os.write(reinterpret_cast<const char*>(&eid),   sizeof(eid));
      os.write(reinterpret_cast<const char*>(&alive),  sizeof(alive));
      os.write(reinterpret_cast<const char*>(&src),    sizeof(src));
      os.write(reinterpret_cast<const char*>(&dst),    sizeof(dst));

      std::string type = alive ? edge->type : "";
      size_t len = type.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      os.write(type.data(), len);

      if (alive) {
        serialize_properties(os, edge->properties);
      } else {
        size_t zero = 0; os.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
      }
    }

    {
      const auto& free_nodes = db.node_id_free_.free_ids();
      size_t len = free_nodes.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      for (auto id : free_nodes) os.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }
    {
      const auto& free_edges = db.edge_id_free_.free_ids();
      size_t len = free_edges.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      for (auto id : free_edges) os.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }

    return os.good();
  }

  bool GraphStorage::load(GraphDB& db, const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) {
      return false;
    }

    FileHeader header;
    is.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!is || header.magic != 0x12345678 || header.version != 2) {
      return false;
    }

    bool dm = db.disk_mode();

    db.node_index_.clear();
    db.edge_index_.clear();
    db.metrics_->clear();
    if (!dm) { db.nodes_ram_.clear(); db.edges_ram_.clear(); }
    if (!dm) {db.nodes_ram_.resize(header.node_count);}

    for (size_t i = 0; i < header.node_count; ++i) {
      NodeId nid; bool alive;
      is.read(reinterpret_cast<char*>(&nid),   sizeof(nid));
      is.read(reinterpret_cast<char*>(&alive),  sizeof(alive));

      size_t label_count;
      is.read(reinterpret_cast<char*>(&label_count), sizeof(label_count));
      if (!is) { return false; }

      Node node;
      node.id = nid;
      node.alive = alive;
      node.labels.resize(label_count);

      for (size_t j = 0; j < label_count; ++j) {
        size_t len;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!is) { return false; }
        std::string label(len, '\0');
        is.read(label.data(), len);
        node.labels[j] = label;
      }

      node.properties = deserialize_properties(is);

      if (alive) {
        for (const auto& label : node.labels)    db.node_index_.add_label(nid, label);
        for (const auto& [k, v] : node.properties) db.node_index_.add_property(nid, k, v);
        db.metrics_->on_node_created(node.labels, node.properties);
      }

      if (dm) {
        db.node_store_->put(node);
      }
      else    db.nodes_ram_[i] = std::move(node);
    }

    if (!dm) {
      db.edges_ram_.resize(header.edge_count);
    }

    for (size_t i = 0; i < header.edge_count; ++i) {
      EdgeId eid; bool alive; NodeId src, dst;
      is.read(reinterpret_cast<char*>(&eid),   sizeof(eid));
      is.read(reinterpret_cast<char*>(&alive),  sizeof(alive));
      is.read(reinterpret_cast<char*>(&src),    sizeof(src));
      is.read(reinterpret_cast<char*>(&dst),    sizeof(dst));

      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));
      if (!is) {
        return false;
      }

      std::string type(len, '\0');
      is.read(type.data(), len);

      Properties props = deserialize_properties(is);

      Edge edge{ eid, alive, src, dst, type, props };

      if (alive) {
        db.edge_index_.add_edge(eid, src, dst, type);
        Node* src_node = dm ? db.node_store_->get(src)
                            : (src < db.nodes_ram_.size() ? &db.nodes_ram_[src] : nullptr);
        std::vector<std::string> src_labels;
        if (src_node && src_node->alive) {
          src_labels = src_node->labels;
        }
        db.metrics_->on_edge_created(src_labels);
      }

      if (dm) {
        db.edge_store_->put(edge);
      } else {
        db.edges_ram_[i] = std::move(edge);
      }
    }

    {
      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));
      for (size_t i = 0; i < len; ++i) {
        size_t id;
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
        db.node_id_free_.free(id);
      }
    }
    if (!is) {
      return false;
    }
    {
      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));
      for (size_t i = 0; i < len; ++i) {
        size_t id;
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
        db.edge_id_free_.free(id);
      }
    }

    return is.good();
  }

  void GraphStorage::serialize_value(std::ostream& os, const Value& val) {
    uint8_t type = static_cast<uint8_t>(val.index());
    os.write(reinterpret_cast<const char*>(&type), sizeof(type));
    switch (val.index()) {
      case 0: {
        auto v = std::get<int64_t>(val);
        os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        break;
      }
      case 1: {
        auto v = std::get<double>(val);
        os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        break;
      }
      case 2: {
        const auto& s = std::get<std::string>(val);
        size_t len = s.size();
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        os.write(s.data(), len); break;
      }
      case 3: {
        auto v = std::get<bool>(val);
        os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        break;
      }
      default: throw std::runtime_error("GraphStorage: unknown value type");
    }
  }

  [[nodiscard]] Value GraphStorage::deserialize_value(std::istream& is) {
    uint8_t type;
    is.read(reinterpret_cast<char*>(&type), sizeof(type));
    switch (type) {
      case 0: {
        int64_t v;
        is.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
      }
      case 1: {
        double v;
        is.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
      }
      case 2: {
        size_t len;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s(len, '\0'); is.read(s.data(), len);
        return s;
      }
      case 3: {
        bool v;
        is.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
      }
      default: throw std::runtime_error("GraphStorage: unknown value type");
    }
  }

  void GraphStorage::serialize_properties(std::ostream& os, const Properties& props) {
    size_t count = props.size();
    os.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& [key, val] : props) {
      size_t len = key.size();
      os.write(reinterpret_cast<const char*>(&len), sizeof(len));
      os.write(key.data(), len);
      serialize_value(os, val);
    }
  }

  [[nodiscard]] Properties GraphStorage::deserialize_properties(std::istream& is) {
    size_t count;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));
    Properties props;
    for (size_t i = 0; i < count; ++i) {
      size_t len;
      is.read(reinterpret_cast<char*>(&len), sizeof(len));
      std::string key(len, '\0');
      is.read(key.data(), len);
      props[std::move(key)] = deserialize_value(is);
    }
    return props;
  }

} // namespace storage