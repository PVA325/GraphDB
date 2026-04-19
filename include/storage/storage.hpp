// include guards?
#pragma once

#include <fstream>
#include <functional>
#include <iostream>
#include <stack>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
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
  std::vector<Id> ids_;                     // вектор ID для перебора
  std::function<bool(T*)> predicate_ = nullptr;    // фильтр, может быть сложный
  size_t index_ = 0;                               // текущая позиция
  size_t limit_ = 0;                               // лимит, 0 = без лимита
  size_t returned_ = 0;                            // сколько уже вернули

public:
  Cursor(GraphDB* db, const std::vector<Id>& ids,
         std::function<bool(T*)> predicate = nullptr,
         size_t limit = 0)
      : db_(db), ids_(ids), predicate_(predicate), limit_(limit) {};

  virtual ~Cursor() = default;

  bool next(T*& out);

  inline void reset() { index_ = 0; returned_ = 0; }

protected:
  virtual T* get_from_db(Id id) = 0;
};

// ===================== NodeCursor =====================
// fix constructors
class NodeCursor : public Cursor<Node, NodeId> {
public:
  NodeCursor(GraphDB* db, const std::vector<NodeId>& node_ids,
             std::function<bool(Node*)> predicate = nullptr,
             size_t limit = 0);

protected:
  Node* get_from_db(NodeId id) override;
};

// ===================== EdgeCursor =====================
class EdgeCursor : public Cursor<Edge, EdgeId> {
public:
  EdgeCursor(GraphDB* db, const std::vector<EdgeId>& edge_ids,
             std::function<bool(Edge*)> predicate = nullptr,
             size_t limit = 0);

protected:
  Edge* get_from_db(EdgeId id) override;
};


class GraphDB {
public:
  GraphDB() = default;

  // ======= CRUD Nodes =======
  NodeId create_node(const std::vector<std::string>& labels, const Properties& props);

  void set_node_property(NodeId id, const std::string& key, const Value& val);

  void delete_node(NodeId id);

  Node* get_node(NodeId id);

  // ======= CRUD Edges =======
  EdgeId create_edge(NodeId src, NodeId dst, const std::string& type, const Properties& props);

  void set_edge_property(EdgeId id, const std::string& key, const Value& val);

  void delete_edge(EdgeId id);

  Edge* get_edge(EdgeId id);

  // Upload/Download
  bool save_to_file(const std::string& path);
  bool load_from_file(const std::string& path);


  // ======= Node Cursors =======
  std::unique_ptr<NodeCursor> all_nodes(std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

  std::unique_ptr<NodeCursor> nodes_with_label(const std::string& label,
                                               std::function<bool(Node*)> predicate = nullptr,
                                               size_t limit = 0);

  std::unique_ptr<NodeCursor> nodes_with_property(const std::string& key, const Value& val,
                                                  std::function<bool(Node*)> predicate = nullptr,
                                                  size_t limit = 0);

  // ======= Edge Cursors =======
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

  size_t node_count_with_label(const std::string& label) const;

  // количество различных значений свойства для узлов с меткой
  std::optional<size_t> property_distinct_count(const std::string& label, const std::string& property) const;

  // среднее количество исходящих ребер для узлов с меткой
  double avg_out_degree(const std::string& label) const;

  // проверка, есть ли индекс по свойству для узлов с меткой
  bool has_property_index(const std::string& label, const std::string& property) const;

private:

  // ===== Graph structure =====
  std::vector<Node> nodes_; // nodes
  std::vector<Edge> edges_; // edges

  std::stack<NodeId> free_node_ids; // node ids for second using
  std::stack<EdgeId> free_edge_ids; // edge ids for second using
  std::unordered_map<std::string,std::vector<NodeId>> label_index_; // label_index

  // в далеком будушем я возможно перепеши это на B+ tree, чтобы фильтр работал быстрее
  std::unordered_map<std::string, std::unordered_map<Value, std::vector<NodeId>>> property_index_; // property_index

  void serialize_value(std::ostream& os, const Value& val);
  Value deserialize_value(std::istream& is);

  void serialize_properties(std::ostream& os, const Properties& props);
  Properties deserialize_properties(std::istream& is);

  // Метод для пересчета всех индексов после загрузки данных
  // (Индексы обычно не сохраняют в файл, чтобы не дублировать данные,
  // их быстрее перестроить в памяти при старте)
  void rebuild_indexes();

  std::unordered_map<NodeId,std::vector<EdgeId>> outgoing_; // adjacency
  std::unordered_map<NodeId,std::vector<EdgeId>> incoming_; // adjacency
  std::unordered_map<std::string,std::vector<EdgeId>> edge_type_index_; // type index
};
}

#include "base_cursor_storage.tpp"