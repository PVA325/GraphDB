#include <cassert>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include "storage/node_store.hpp"
#include "storage/serialise.hpp"

namespace storage {

  using namespace serial;

  static std::fstream open_file(const std::string& path) {
    if (!std::filesystem::exists(path)) {
      std::ofstream{path, std::ios::binary};
    }
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) { throw std::runtime_error("cannot open " + path); }
    return f;
  }

  NodeStore::NodeStore(const std::string& dir)
    : slots_file_(open_file(dir + "/nodes.dat")),
      props_file_ (open_file(dir + "/node_props.dat")),
      slots_cache_(slots_file_, kMaxSlotsPageAmount),
      props_cache_(props_file_, kMaxPropsPageAmount)
  {
    slots_file_.seekg(0, std::ios::end);
    slot_count_ = static_cast<size_t>(slots_file_.tellg()) / NodeSlot::kSize;

    props_file_.seekg(0, std::ios::end);
    props_end_ = static_cast<size_t>(props_file_.tellg());
  }

  NodeStore::~NodeStore() { flush(); }

  void NodeStore::put(NodeId id, const Node& node) {
    size_t props_offset = serialise(node);

    NodeSlot slot;
    slot.alive = true;
    slot.props_offset = props_offset;
    slot.props_size = static_cast<uint32_t>(props_end_ - props_offset);
    write_slot(id, slot);

    if (obj_cache_.size() >= kMaxNodeAmount) { evict_obj_cache(); }
    obj_cache_[id] = node;
    obj_cache_[id].id = id;
    obj_cache_[id].alive = true;
  }

  Node* NodeStore::get(NodeId id) {
    if (id >= slot_count_) return nullptr;

    auto it = obj_cache_.find(id);
    if (it != obj_cache_.end()) {
      return it->second.alive ? &it->second : nullptr;
    }

    NodeSlot slot = read_slot(id);
    if (!slot.alive) { return nullptr; }

    if (obj_cache_.size() >= kMaxNodeAmount) { evict_obj_cache(); }
    obj_cache_[id] = deserialise(id, slot);
    return &obj_cache_[id];
  }

  void NodeStore::remove(NodeId id) {
    assert(id < slot_count_);
    NodeSlot slot = read_slot(id);
    slot.alive = false;
    write_slot(id, slot);
    obj_cache_.erase(id);
  }

  void NodeStore::flush() {
    slots_cache_.flush();
    props_cache_.flush();
  }

  NodeSlot NodeStore::read_slot(NodeId id) {
    NodeSlot slot;
    size_t   offset = id * NodeSlot::kSize;
    slots_cache_.read(offset,     &slot.props_offset, 8);
    slots_cache_.read(offset + 8, &slot.props_size,   4);
    uint8_t alive = 0;
    slots_cache_.read(offset + 12, &alive, 1);
    slot.alive = alive != 0;
    return slot;
  }

  void NodeStore::write_slot(NodeId id, const NodeSlot& slot) {
    size_t  offset = id * NodeSlot::kSize;
    uint8_t alive  = slot.alive ? 1 : 0;
    slots_cache_.write(offset,      &slot.props_offset, 8);
    slots_cache_.write(offset +  8, &slot.props_size,   4);
    slots_cache_.write(offset + 12, &alive,             1);
    if (id >= slot_count_) { slot_count_ = id + 1; }
  }

  size_t NodeStore::serialise(const Node& node) {
    std::ostringstream buf(std::ios::binary);
    write<uint32_t>(buf, static_cast<uint32_t>(node.labels.size()));
    for (const auto& label : node.labels) { write_str(buf, label); }
    write_properties(buf, node.properties);

    std::string data = buf.str();
    size_t offset = props_end_;
    props_cache_.write(props_end_, data.data(), data.size());
    props_end_ += data.size();
    return offset;
  }

  Node NodeStore::deserialise(NodeId id, const NodeSlot& slot) {
    std::string buf(slot.props_size, '\0');
    props_cache_.read(slot.props_offset, buf.data(), slot.props_size);
    std::istringstream is(buf, std::ios::binary);

    Node node;
    node.id = id;
    node.alive = true;

    uint32_t label_count = read<uint32_t>(is);
    node.labels.resize(label_count);
    for (uint32_t i = 0; i < label_count; ++i) { node.labels[i] = read_str(is); }
    node.properties = read_properties(is);
    return node;
  }

  void NodeStore::evict_obj_cache() {
    if (!obj_cache_.empty()) { obj_cache_.erase(obj_cache_.begin()); }
  }

} // namespace storage