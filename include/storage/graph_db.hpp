#pragma once

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "cursor.hpp"
#include "EdgeEntity/edge_manager.hpp"
#include "graph_storage.hpp"
#include "free_list.hpp"
#include "metrics_store.hpp"
#include "NodeEntity/node_manager.hpp"
#include "types.hpp"

namespace storage {
  class GraphDB {
  public:
    explicit GraphDB(const std::filesystem::path& dir = "");
    ~GraphDB();

    NodeId create_node(const std::vector<std::string>& labels,
                            const Properties& props);
    Node*  get_node(NodeId id);
    void set_node_property(NodeId id, const std::string& key, const Value& val);
    void set_node_label(NodeId id, const std::string& label);
    void delete_node_label(NodeId id, const std::string& label);
    void delete_node(NodeId id);

    EdgeId create_edge(NodeId src, NodeId dst,
                       const std::string& type, const Properties& props);
    Edge* get_edge(EdgeId id);
    void set_edge_property(EdgeId id, const std::string& key, const Value& val);
    void set_edge_type(EdgeId id, const std::string& type);
    void delete_edge(EdgeId id);

    bool save_to_file(const std::string& path) const {
      return GraphStorage::save(*this, path);
    }
    bool load_from_file(const std::string& path) {
      return GraphStorage::load(*this, path);
    }
    void flush();

    [[nodiscard]] std::unique_ptr<NodeCursor> all_nodes(
      std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] std::unique_ptr<NodeCursor> nodes_with_label(
      const std::string& label,
      std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] std::unique_ptr<NodeCursor> nodes_with_property(
      const std::string& key, const Value& val,
      std::function<bool(Node*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] std::unique_ptr<EdgeCursor> outgoing_edges(
      NodeId node_id,
      std::function<bool(Edge*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] std::unique_ptr<EdgeCursor> incoming_edges(
      NodeId node_id,
      std::function<bool(Edge*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] std::unique_ptr<EdgeCursor> edges_by_type(
      const std::string& type,
      std::function<bool(Edge*)> predicate = nullptr, size_t limit = 0);

    [[nodiscard]] size_t node_count() const;
    [[nodiscard]] size_t edge_count_with_type  (const std::string& type) const;
    [[nodiscard]] size_t node_count_with_label (const std::string& label) const;
    [[nodiscard]] std::optional<size_t> property_distinct_count(
                        const std::string& property, const std::string& label) const;
    [[nodiscard]] double avg_out_degree(const std::string& label) const;
    [[nodiscard]] bool has_property_index(const std::string& label,
                                          const std::string& property) const;
    [[nodiscard]] size_t property_count(const std::string& property,
                                        const Value& value,
                                        const std::string& label) const;

    friend class GraphStorage;
    friend class AllNodesCursor;

  private:
    [[nodiscard]] bool disk_mode() const { return !dir_.empty(); }

    std::filesystem::path dir_;

    std::unique_ptr<NodeStore> node_store_;
    std::unique_ptr<EdgeStore> edge_store_;
    std::deque<Node> nodes_ram_;
    std::deque<Edge> edges_ram_;

    FreeList node_id_free_;
    FreeList edge_id_free_;
    NodeIndex node_index_;
    EdgeIndex edge_index_;

    std::unique_ptr<MetricsStore> metrics_;
    std::unique_ptr<NodeManager> node_manager_;
    std::unique_ptr<EdgeManager> edge_manager_;
    std::unique_ptr<CursorFactory> cursor_factory_;

    Node* node_ptr(NodeId id);
    Edge* edge_ptr(EdgeId id);
  };

} // namespace storage