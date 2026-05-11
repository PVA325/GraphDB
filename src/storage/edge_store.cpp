#include <cassert>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include "storage/edge_store.hpp"
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

  EdgeStore::EdgeStore(const std::string& dir)
    : slots_file_(open_file(dir + "/edges.dat")),
      props_file_ (open_file(dir + "/edge_props.dat")),
      slots_cache_(slots_file_, kMaxSlotsPageAmount),
      props_cache_(props_file_, kMaxPropsPageAmount)
  {
    slots_file_.seekg(0, std::ios::end);
    slot_count_ = static_cast<size_t>(slots_file_.tellg()) / EdgeSlot::kSize;

    props_file_.seekg(0, std::ios::end);
    props_end_ = static_cast<size_t>(props_file_.tellg());
  }

  EdgeStore::~EdgeStore() { flush(); }

  void EdgeStore::put(EdgeId id, const Edge& edge) {
    size_t props_offset = serialise(edge);

    EdgeSlot slot;
    slot.alive = true;
    slot.props_offset = props_offset;
    slot.props_size = static_cast<uint32_t>(props_end_ - props_offset);
    slot.src = edge.src;
    slot.dst = edge.dst;
    write_slot(id, slot);

    if (obj_cache_.size() >= kMaxNodeAmount) {evict_obj_cache();}
    obj_cache_[id] = edge;
    obj_cache_[id].id = id;
    obj_cache_[id].alive = true;
  }

  Edge* EdgeStore::get(EdgeId id) {
    if (id >= slot_count_) {return nullptr;}

    auto it = obj_cache_.find(id);
    if (it != obj_cache_.end()) {
      return it->second.alive ? &it->second : nullptr;
    }

    EdgeSlot slot = read_slot(id);
    if (!slot.alive) {return nullptr;}

    if (obj_cache_.size() >= kMaxNodeAmount) {evict_obj_cache();}
    obj_cache_[id] = deserialise(id, slot);
    return &obj_cache_[id];
  }

  void EdgeStore::remove(EdgeId id) {
    assert(id < slot_count_);
    EdgeSlot slot = read_slot(id);
    slot.alive = false;
    write_slot(id, slot);
    obj_cache_.erase(id);
  }

  void EdgeStore::flush() {
    slots_cache_.flush();
    props_cache_.flush();
  }


  EdgeSlot EdgeStore::read_slot(EdgeId id) {
    EdgeSlot slot;
    size_t   offset = id * EdgeSlot::kSize;
    slots_cache_.read(offset,      &slot.props_offset, 8);
    slots_cache_.read(offset +  8, &slot.src,          8);
    slots_cache_.read(offset + 16, &slot.dst,          8);
    slots_cache_.read(offset + 24, &slot.props_size,   4);
    uint8_t alive = 0;
    slots_cache_.read(offset + 28, &alive, 1);
    slot.alive = alive != 0;
    return slot;
  }

  void EdgeStore::write_slot(EdgeId id, const EdgeSlot& slot) {
    size_t  offset = id * EdgeSlot::kSize;
    uint8_t alive  = slot.alive ? 1 : 0;
    slots_cache_.write(offset,      &slot.props_offset, 8);
    slots_cache_.write(offset +  8, &slot.src,          8);
    slots_cache_.write(offset + 16, &slot.dst,          8);
    slots_cache_.write(offset + 24, &slot.props_size,   4);
    slots_cache_.write(offset + 28, &alive,             1);
    if (id >= slot_count_) { slot_count_ = id + 1; }
  }

  size_t EdgeStore::serialise(const Edge& edge) {
    std::ostringstream buf(std::ios::binary);
    write_str(buf, edge.type);
    write_properties(buf, edge.properties);

    std::string data   = buf.str();
    size_t      offset = props_end_;
    props_cache_.write(props_end_, data.data(), data.size());
    props_end_ += data.size();
    return offset;
  }

  Edge EdgeStore::deserialise(EdgeId id, const EdgeSlot& slot) {
    std::string buf(slot.props_size, '\0');
    props_cache_.read(slot.props_offset, buf.data(), slot.props_size);
    std::istringstream is(buf, std::ios::binary);

    Edge edge;
    edge.id = id;
    edge.alive = true;
    edge.src = slot.src;
    edge.dst = slot.dst;
    edge.type = read_str(is);
    edge.properties = read_properties(is);
    return edge;
  }

  void EdgeStore::evict_obj_cache() {
    if (!obj_cache_.empty()) {obj_cache_.erase(obj_cache_.begin());}
  }

} // namespace storage