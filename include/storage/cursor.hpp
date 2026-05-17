#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "EdgeEntity/edge_index.hpp"
#include "NodeEntity/node_index.hpp"
#include "types.hpp"

namespace storage {

  class GraphDB;

  template<typename T, typename Id>
  class Cursor {
  protected:
    GraphDB* db_;
    const std::vector<Id>& ids_;
    std::function<bool(T*)> predicate_ = nullptr;
    size_t index_ = 0;
    size_t limit_ = 0;
    size_t returned_ = 0;

  public:
    Cursor(GraphDB* db, const std::vector<Id>& ids,
           std::function<bool(T*)> predicate = nullptr, size_t limit = 0)
      : db_(db), ids_(ids), predicate_(predicate), limit_(limit) {}

    virtual ~Cursor() = default;
    virtual bool next(T*& out);
    void reset() { index_ = 0; returned_ = 0; }

  protected:
    virtual T* get_from_db(Id id) = 0;
  };

  class NodeCursor : public Cursor<Node, NodeId> {
  public:
    NodeCursor(GraphDB* db, const std::vector<NodeId>& ids,
               std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

  protected:
    [[nodiscard]] Node* get_from_db(NodeId id) override;
  };

  class EdgeCursor : public Cursor<Edge, EdgeId> {
  public:
    EdgeCursor(GraphDB* db, const std::vector<EdgeId>& ids,
               std::function<bool(Edge*)> predicate = nullptr, size_t limit = 0);

  protected:
    Edge* get_from_db(EdgeId id) override;
  };

  class AllNodesCursor : public NodeCursor {
  public:
    AllNodesCursor(GraphDB* db,
                   std::function<bool(Node*)> predicate = nullptr,
                   size_t limit = 0,
                   bool disk_mode = false,
                   size_t total_slots = 0)
      : NodeCursor(db,std::vector<NodeId>{}, predicate, limit),
        disk_mode_(disk_mode), total_slots_(total_slots) {}
    bool next(Node*& out) override;

  private:
    bool disk_mode_ = false;
    size_t total_slots_ = 0;
  };

  class CursorFactory {
  public:
    CursorFactory(GraphDB* db, NodeIndex* node_index, EdgeIndex* edge_index);

    std::unique_ptr<NodeCursor> all_nodes(
      std::function<bool(Node*)> predicate, size_t limit);

    std::unique_ptr<NodeCursor> nodes_with_label(
      const std::string& label,
      std::function<bool(Node*)> predicate, size_t limit);

    std::unique_ptr<NodeCursor> nodes_with_property(
      const std::string& key, const Value& val,
      std::function<bool(Node*)> predicate, size_t limit);

    std::unique_ptr<EdgeCursor> outgoing_edges(
      NodeId node_id,
      std::function<bool(Edge*)> predicate, size_t limit);

    std::unique_ptr<EdgeCursor> incoming_edges(
      NodeId node_id,
      std::function<bool(Edge*)> predicate, size_t limit);

    std::unique_ptr<EdgeCursor> edges_by_type(
      const std::string& type,
      std::function<bool(Edge*)> predicate, size_t limit);

  private:
    GraphDB*   db_;
    NodeIndex* node_index_;
    EdgeIndex* edge_index_;
  };

} // namespace storage

#include "base_cursor_storage.tpp"