// include guards?
#pragma once

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

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

class GraphDB;

  template<typename T, typename Id>
  class Cursor {
  protected:
    GraphDB* db_;
    std::optional<std::vector<Id>> owned_ids_;
    const std::vector<Id>* ids_;
    std::function<bool(T*)> predicate_ = nullptr;
    size_t index_ = 0;
    size_t limit_ = 0;
    size_t returned_ = 0;

  public:
    inline Cursor(GraphDB* db, const std::vector<Id>& ids,
                  std::function<bool(T*)> predicate = nullptr,
                  size_t limit = 0)
      : db_(db), ids_(&ids), predicate_(predicate), limit_(limit) {}

    inline Cursor(GraphDB* db, std::vector<Id>&& ids,
                  std::function<bool(T*)> predicate = nullptr,
                  size_t limit = 0)
      : db_(db), owned_ids_(std::move(ids)), ids_(&*owned_ids_),
        predicate_(predicate), limit_(limit) {}

    virtual ~Cursor() = default;

    bool next(T*& out);

    inline void reset() { index_ = 0; returned_ = 0; }

  protected:
    virtual T* get_from_db(Id id) = 0;
  };

class NodeCursor : public Cursor<Node, NodeId> {
public:
  NodeCursor(GraphDB* db, const std::vector<NodeId>& node_ids,
             std::function<bool(Node*)> predicate = nullptr,
             size_t limit = 0);

protected:
  Node* get_from_db(NodeId id) override;
};

class EdgeCursor : public Cursor<Edge, EdgeId> {
public:
  EdgeCursor(GraphDB* db, const std::vector<EdgeId>& edge_ids,
             std::function<bool(Edge*)> predicate = nullptr,
             size_t limit = 0);

protected:
  Edge* get_from_db(EdgeId id) override;
};

class GraphStorage {
public:
  static bool save(const GraphDB& db, const std::string& path);
  static bool load(GraphDB& db, const std::string& path);

private:
  struct FileHeader {
    uint32_t magic = 0x12345678;
    uint16_t version = 1;
    uint16_t header_size = sizeof(FileHeader);

    uint64_t node_count = 0;
    uint64_t edge_count = 0;
  };

  static void serialize_value(std::ostream& os, const Value& val);
  static Value deserialize_value(std::istream& is);

  static void serialize_properties(std::ostream& os, const Properties& props);
  static Properties deserialize_properties(std::istream& is);
};


class GraphDB {
public:
  GraphDB() = default;

  NodeId create_node(const std::vector<std::string>& labels, const Properties& props);

  void set_node_property(NodeId id, const std::string& key, const Value& val);

  void set_node_label(NodeId, const std::string& label);

  void delete_node(NodeId id);

  void delete_node_label(NodeId id, const std::string& label);

  Node* get_node(NodeId id);

  EdgeId create_edge(NodeId src, NodeId dst, const std::string& type, const Properties& props);

  void set_edge_property(EdgeId id, const std::string& key, const Value& val);

  void delete_edge(EdgeId id);

  void set_edge_type(EdgeId id, const std::string& type);

  Edge* get_edge(EdgeId id);

  friend class GraphStorage;

  inline bool save_to_file(const std::string& path) const {
    return GraphStorage::save(*this, path);
  }

  inline bool load_from_file(const std::string& path) {
    return GraphStorage::load(*this, path);
  }

  std::unique_ptr<NodeCursor> all_nodes(std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

  std::unique_ptr<NodeCursor> nodes_with_label(const std::string& label,
                                               std::function<bool(Node*)> predicate = nullptr,
                                               size_t limit = 0);

  std::unique_ptr<NodeCursor> nodes_with_property(const std::string& key, const Value& val,
                                                  std::function<bool(Node*)> predicate = nullptr,
                                                  size_t limit = 0);

  std::unique_ptr<EdgeCursor> outgoing_edges(NodeId node_id,
                                             std::function<bool(Edge*)> predicate = nullptr,
                                             size_t limit = 0);

  std::unique_ptr<EdgeCursor> incoming_edges(NodeId node_id,
                                             std::function<bool(Edge*)> predicate = nullptr,
                                             size_t limit = 0);

  std::unique_ptr<EdgeCursor> edges_by_type(const std::string& type,
                                            std::function<bool(Edge*)> predicate = nullptr,
                                            size_t limit = 0);

  size_t node_count() const;

  size_t edge_count_with_type(const std::string& type) const;

  size_t node_count_with_label(const std::string& label) const;

  std::optional<size_t> property_distinct_count(const std::string& property, const std::string& label) const;

  double avg_out_degree(const std::string& label) const;

  bool has_property_index(const std::string& label, const std::string& property) const;

  size_t property_count(const std::string& property,const Value& value, const std::string& label) const;

private:

  std::vector<Node> nodes_;
  std::vector<Edge> edges_;

  std::vector<NodeId> free_node_ids;
  std::vector<EdgeId> free_edge_ids;
  std::unordered_map<std::string,std::vector<NodeId>> label_index_;

  std::unordered_map<std::string, std::unordered_map<Value, std::vector<NodeId>>> property_index_;

  std::unordered_map<NodeId,std::vector<EdgeId>> outgoing_;
  std::unordered_map<NodeId,std::vector<EdgeId>> incoming_;
  std::unordered_map<std::string,std::vector<EdgeId>> edge_type_index_;

  std::unordered_map<std::string,
    std::unordered_set<Value>> property_distinct_;

  std::unordered_map<std::string,
    std::unordered_map<std::string,
      std::unordered_set<Value>>> label_property_distinct_;


  std::unordered_map<std::string,std::unordered_map<std::string,
      std::unordered_map<Value, size_t>>> label_property_count_;
  std::unordered_map<std::string, size_t> label_total_out_degree_;
};
}

#include "base_cursor_storage.tpp"