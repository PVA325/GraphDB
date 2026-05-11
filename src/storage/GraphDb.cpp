#include <filesystem>
#include <cassert>

#include "storage/graph_db.hpp"

namespace storage {
  GraphDB::GraphDB(const std::string& dir)
    : dir_(dir)
  {
    metrics_ = std::make_unique<MetricsStore>(dir_);

    if (disk_mode()) {
      std::filesystem::create_directories(dir_);
      node_store_ = std::make_unique<NodeStore>(dir_);
      edge_store_ = std::make_unique<EdgeStore>(dir_);
    }

    node_manager_ = std::make_unique<NodeManager>(
      node_store_.get(), &node_index_, &edge_index_, metrics_.get(), &node_id_free_);

    edge_manager_ = std::make_unique<EdgeManager>(
      edge_store_.get(), &edge_index_, node_store_.get(), metrics_.get(), &edge_id_free_);

    cursor_factory_ = std::make_unique<CursorFactory>(this, &node_index_, &edge_index_);
  }

  GraphDB::~GraphDB() {
    flush();
  }

  void GraphDB::flush() {
    if (disk_mode()) {
      node_store_->flush();
      edge_store_->flush();
    }
    metrics_->flush();
  }

  Node* GraphDB::node_ptr(NodeId id) {
    if (disk_mode()) { return node_store_->get(id); }
    if (id >= nodes_ram_.size() || !nodes_ram_[id].alive) { return nullptr; }
    return &nodes_ram_[id];
  }

  Edge* GraphDB::edge_ptr(EdgeId id) {
    if (disk_mode()) { return edge_store_->get(id); }
    if (id >= edges_ram_.size() || !edges_ram_[id].alive) { return nullptr; }
    return &edges_ram_[id];
  }

  NodeId GraphDB::create_node(const std::vector<std::string>& labels,
                              const Properties& props) {
    if (disk_mode()) {
      return node_manager_->create(labels, props);
    }

    NodeId id = node_id_free_.next(nodes_ram_.size());
    Node node{ id, true, labels, props };

    if (id == nodes_ram_.size()) {
      nodes_ram_.push_back(std::move(node));
    }
    else {
      nodes_ram_[id] = std::move(node);
    }

    for (const auto& label : labels) { node_index_.add_label(id, label); }
    for (const auto& [k, v] : props) { node_index_.add_property(id, k, v); }
    metrics_->on_node_created(labels, props);

    return id;
  }

  Node* GraphDB::get_node(NodeId id) {
    return node_ptr(id);
  }

  void GraphDB::set_node_property(NodeId id, const std::string& key, const Value& val) {
    if (disk_mode()) {
      node_manager_->set_property(id, key, val);
      return;
    }

    Node* node = node_ptr(id);
    assert(node && node->alive);

    auto it = node->properties.find(key);
    if (it != node->properties.end()) node_index_.update_property(id, key, it->second, val);
    else                               node_index_.add_property(id, key, val);

    node->properties[key] = val;
  }

  void GraphDB::set_node_label(NodeId id, const std::string& label) {
    if (disk_mode()) {
      node_manager_->add_label(id, label);
      return;
    }

    Node* node = node_ptr(id);
    assert(node && node->alive);

    if (std::find(node->labels.begin(), node->labels.end(), label) != node->labels.end())
      return;

    node->labels.push_back(label);
    node_index_.add_label(id, label);
    metrics_->on_label_added(label, node->properties, edge_index_.out_degree(id));
  }

  void GraphDB::delete_node_label(NodeId id, const std::string& label) {
    if (disk_mode()) {
      node_manager_->remove_label(id, label);
      return;
    }

    Node* node = node_ptr(id);
    assert(node && node->alive);

    node->labels.erase(
      std::remove(node->labels.begin(), node->labels.end(), label),
      node->labels.end());

    node_index_.remove_label(id, label);
    metrics_->on_label_removed(label, edge_index_.out_degree(id));
  }

  void GraphDB::delete_node(NodeId id) {
    if (disk_mode()) {
      auto out = edge_index_.outgoing(id);
      if (out) {
        auto edges = out->data;
        for (EdgeId eid : edges) edge_manager_->remove(eid);
      }
      auto in = edge_index_.incoming(id);
      if (in) {
        auto edges = in->data;
        for (EdgeId eid : edges) edge_manager_->remove(eid);
      }
      node_manager_->remove(id);
      return;
    }

    Node* node = node_ptr(id);
    assert(node && node->alive);

    auto out = edge_index_.outgoing(id);
    if (out) { auto edges = out->data; for (EdgeId eid : edges) delete_edge(eid); }
    auto in  = edge_index_.incoming(id);
    if (in)  { auto edges = in->data;  for (EdgeId eid : edges) delete_edge(eid); }

    for (const auto& label : node->labels)    node_index_.remove_label(id, label);
    for (const auto& [k, v] : node->properties) node_index_.remove_property(id, k, v);

    metrics_->on_node_deleted(node->labels, node->properties);

    node->labels.clear();
    node->properties.clear();
    node->alive = false;
    node_id_free_.free(id);
  }


  EdgeId GraphDB::create_edge(NodeId src, NodeId dst,
                              const std::string& type, const Properties& props) {
    if (disk_mode()) {
      return edge_manager_->create(src, dst, type, props);
    }

    EdgeId id = edge_id_free_.next(edges_ram_.size());
    Edge edge{ id, true, src, dst, type, props };

    if (id == edges_ram_.size()) edges_ram_.push_back(std::move(edge));
    else                         edges_ram_[id] = std::move(edge);

    edge_index_.add_edge(id, src, dst, type);

    std::vector<std::string> src_labels;
    if (src < nodes_ram_.size() && nodes_ram_[src].alive)
      src_labels = nodes_ram_[src].labels;
    metrics_->on_edge_created(src_labels);

    return id;
  }

  Edge* GraphDB::get_edge(EdgeId id) {
    return edge_ptr(id);
  }

  void GraphDB::set_edge_property(EdgeId id, const std::string& key, const Value& val) {
    if (disk_mode()) { edge_manager_->set_property(id, key, val); return; }
    Edge* edge = edge_ptr(id);
    assert(edge && edge->alive);
    edge->properties[key] = val;
  }

  void GraphDB::set_edge_type(EdgeId id, const std::string& type) {
    if (disk_mode()) { edge_manager_->set_type(id, type); return; }
    Edge* edge = edge_ptr(id);
    assert(edge && edge->alive);
    edge_index_.retype_edge(id, edge->type, type);
    edge->type = type;
  }

  void GraphDB::delete_edge(EdgeId id) {
    if (disk_mode()) { edge_manager_->remove(id); return; }

    Edge* edge = edge_ptr(id);
    assert(edge && edge->alive);

    std::vector<std::string> src_labels;
    if (edge->src < nodes_ram_.size() && nodes_ram_[edge->src].alive)
      src_labels = nodes_ram_[edge->src].labels;

    edge_index_.remove_edge(id, edge->src, edge->dst, edge->type);
    metrics_->on_edge_deleted(src_labels);

    edge->properties.clear();
    edge->type.clear();
    edge->alive = false;
    edge_id_free_.free(id);
  }

  std::unique_ptr<NodeCursor> GraphDB::all_nodes(
    std::function<bool(Node*)> predicate, size_t limit) {

    bool   dm    = disk_mode();
    size_t slots = dm ? node_store_->slot_count() : nodes_ram_.size();
    return std::unique_ptr<NodeCursor>(
      new AllNodesCursor(this, predicate, limit, dm, slots));
  }

  std::unique_ptr<NodeCursor> GraphDB::nodes_with_label(
    const std::string& label,
    std::function<bool(Node*)> predicate, size_t limit) {
    return cursor_factory_->nodes_with_label(label, predicate, limit);
  }

  std::unique_ptr<NodeCursor> GraphDB::nodes_with_property(
    const std::string& key, const Value& val,
    std::function<bool(Node*)> predicate, size_t limit) {
    return cursor_factory_->nodes_with_property(key, val, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> GraphDB::outgoing_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate, size_t limit) {
    return cursor_factory_->outgoing_edges(node_id, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> GraphDB::incoming_edges(
    NodeId node_id,
    std::function<bool(Edge*)> predicate, size_t limit) {
    return cursor_factory_->incoming_edges(node_id, predicate, limit);
  }

  std::unique_ptr<EdgeCursor> GraphDB::edges_by_type(
    const std::string& type,
    std::function<bool(Edge*)> predicate, size_t limit) {
    return cursor_factory_->edges_by_type(type, predicate, limit);
  }


  size_t GraphDB::node_count() const {
    return metrics_->node_count();
  }

  size_t GraphDB::edge_count_with_type(const std::string& type) const {
    return edge_index_.count_by_type(type);
  }

  size_t GraphDB::node_count_with_label(const std::string& label) const {
    return metrics_->node_count_with_label(label);
  }

  std::optional<size_t> GraphDB::property_distinct_count(
    const std::string& property, const std::string& label) const {
    return metrics_->property_distinct_count(property, label);
  }

  double GraphDB::avg_out_degree(const std::string& label) const {
    return metrics_->avg_out_degree(label);
  }

  bool GraphDB::has_property_index(const std::string& label,
                                   const std::string& property) const {
    return metrics_->has_property_index(label, property);
  }

  size_t GraphDB::property_count(const std::string& property,
                                 const Value& value,
                                 const std::string& label) const {
    if (label.empty()) {
      auto list = node_index_.by_property(property, value);
      return list ? list->data.size() : 0;
    }
    return metrics_->property_count(property, value, label);
  }

} // namespace storage