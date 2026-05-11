#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "metrics_store.hpp"
#include "serialise.hpp"

namespace storage {
  size_t LPVKeyHash::operator()(const LPVKey& k) const {
    size_t h = std::hash<uint32_t>{}(k.label_id);
    h ^= std::hash<uint32_t>{}(k.prop_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<Value>{}(k.value) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }

  MetricsStore::MetricsStore(const std::string& dir)
    : dir_(dir) {}

  MetricsStore::~MetricsStore() {
    flush();
  }

  void MetricsStore::on_node_created(const std::vector<std::string>& labels,
                                     const Properties& props) {
    ++total_nodes_;
    for (const auto& label : labels) {
      ++label_node_count_[label];
      for (const auto& [prop, val] : props) {
        LPVKey lpv = make_lpv(label, prop, val);
        ++delta_.count_delta[lpv];
        delta_.new_distinct[make_lp(label, prop)].insert(val);
        ++delta_.write_count;
      }
    }

    for (const auto& [prop, val] : props) {
      property_distinct_[prop].insert(val);
    }
    maybe_flush();
  }

  void MetricsStore::on_node_deleted(const std::vector<std::string>& labels,
                                     const Properties& props) {
    assert(total_nodes_ > 0);
    --total_nodes_;
    for (const auto& label : labels) {
      auto it = label_node_count_.find(label);
      if (it != label_node_count_.end() && it->second > 0) {
        --it->second;
      }

      for (const auto& [prop, val] : props) {
        LPVKey lpv = make_lpv(label, prop, val);
        --delta_.count_delta[lpv];
        ++delta_.write_count;
      }
    }
    maybe_flush();
  }

  void MetricsStore::on_label_added(const std::string& label,
                                    const Properties& node_props,
                                    size_t out_degree) {
    ++label_node_count_[label];
    label_total_out_degree_[label] += out_degree;

    for (const auto& [prop, val] : node_props) {
      LPVKey lpv = make_lpv(label, prop, val);
      delta_.count_delta[lpv]++;
      delta_.new_distinct[make_lp(label, prop)].insert(val);
      ++delta_.write_count;
    }
    maybe_flush();
  }

  void MetricsStore::on_label_removed(const std::string& label, size_t out_degree) {
    auto it = label_node_count_.find(label);
    if (it != label_node_count_.end() && it->second > 0) {
      --it->second;
    }

    auto oit = label_total_out_degree_.find(label);
    if (oit != label_total_out_degree_.end() && oit->second >= out_degree) {
      oit->second -= out_degree;
    }
  }

  void MetricsStore::on_edge_created(const std::vector<std::string>& src_labels) {
    for (const auto& label : src_labels) {
      ++label_total_out_degree_[label];
    }
  }

  void MetricsStore::on_edge_deleted(const std::vector<std::string>& src_labels) {
    for (const auto& label : src_labels) {
      auto it = label_total_out_degree_.find(label);
      if (it != label_total_out_degree_.end() && it->second > 0) {
        --it->second;
      }
    }
  }

  size_t MetricsStore::node_count() const {
    return total_nodes_;
  }

  size_t MetricsStore::node_count_with_label(const std::string& label) const {
    auto it = label_node_count_.find(label);
    return it != label_node_count_.end() ? it->second : 0;
  }

  std::optional<size_t> MetricsStore::property_distinct_count(
    const std::string& prop, const std::string& label) const {

    if (label.empty()) {
      auto it = property_distinct_.find(prop);
      if (it == property_distinct_.end()) {
        return 0;
      }
      return it->second.size();
    }

    uint32_t lid = label_interner_.find(label);
    uint32_t pid = prop_interner_.find(prop);
    if (lid == StringInterner::kUnvalidID || pid == StringInterner::kUnvalidID) {
      return 0;
    }

    LPKey lp{lid, pid};

    size_t delta_distinct = 0;
    auto dit = delta_.new_distinct.find(lp);
    if (dit != delta_.new_distinct.end()) {
      delta_distinct = dit->second.size();
    }

    auto it = lp_distinct_.find(lp);
    if (it == lp_distinct_.end()) {
      return delta_distinct;
    }
    return it->second.size() + delta_distinct;
  }

  double MetricsStore::avg_out_degree(const std::string& label) const {
    auto cnt_it = label_node_count_.find(label);
    if (cnt_it == label_node_count_.end() || cnt_it->second == 0) {
      return 0.0;
    }
    auto deg_it = label_total_out_degree_.find(label);
    if (deg_it == label_total_out_degree_.end()) {
      return 0.0;
    }
    return static_cast<double>(deg_it->second) / cnt_it->second;
  }

  bool MetricsStore::has_property_index(const std::string& label,
                                        const std::string& prop) const {
    uint32_t lid = label_interner_.find(label);
    uint32_t pid = prop_interner_.find(prop);
    if (lid == StringInterner::kUnvalidID || pid == StringInterner::kUnvalidID) {
      return false;
    }
    LPKey lp{lid, pid};
    return lp_distinct_.count(lp) > 0 || delta_.new_distinct.count(lp) > 0;
  }

  size_t MetricsStore::property_count(const std::string& prop,
                                      const Value& value,
                                      const std::string& label) const {
    if (label.empty()) {
      return 0;
    }

    uint32_t lid = label_interner_.find(label);
    uint32_t pid = prop_interner_.find(prop);
    if (lid == StringInterner::kUnvalidID || pid == StringInterner::kUnvalidID) {
      return 0;
    }

    LPVKey lpv{lid, pid, value};

    size_t base = 0;
    auto it = lpv_count_.find(lpv);
    if (it != lpv_count_.end()) {
      base = it->second;
    }

    auto dit = delta_.count_delta.find(lpv);
    if (dit != delta_.count_delta.end()) {
      int64_t total = static_cast<int64_t>(base) + dit->second;
      return total > 0 ? static_cast<size_t>(total) : 0;
    }
    return base;
  }

  void MetricsStore::flush() {
    if (delta_.empty()) { return; }
    apply_delta();
    if (!dir_.empty()) { persist_delta(); }
    delta_.clear();
  }

  void MetricsStore::clear() {
    total_nodes_ = 0;
    label_node_count_.clear();
    label_total_out_degree_.clear();
    property_distinct_.clear();
    lpv_count_.clear();
    lp_distinct_.clear();
    delta_.clear();
  }

  LPVKey MetricsStore::make_lpv(const std::string& label,
                                const std::string& prop,
                                const Value& val) {
    return LPVKey{
      label_interner_.intern(label),
      prop_interner_.intern(prop),
      val
    };
  }

  LPKey MetricsStore::make_lp(const std::string& label, const std::string& prop) {
    return LPKey{
      label_interner_.intern(label),
      prop_interner_.intern(prop)
    };
  }

  void MetricsStore::apply_delta() {
    for (const auto& [key, delta] : delta_.count_delta) {
      int64_t cur = static_cast<int64_t>(lpv_count_[key]);
      cur += delta;
      lpv_count_[key] = cur > 0 ? static_cast<size_t>(cur) : 0;
    }
    for (const auto& [lp, vals] : delta_.new_distinct) {
      for (const auto& v : vals) {
        lp_distinct_[lp].insert(v);
      }
    }
  }

  void MetricsStore::persist_delta() {
    if (dir_.empty()) return;

    std::string path = dir_ + "/metrics_delta.log";
    std::ofstream os(path, std::ios::binary | std::ios::app);
    if (!os) return;

    for (const auto& [key, delta] : delta_.count_delta) {
      serial::write<uint32_t>(os, key.label_id);
      serial::write<uint32_t>(os, key.prop_id);
      serial::write_value    (os, key.value);
      serial::write<int32_t> (os, delta);
    }
  }

  void MetricsStore::maybe_flush() {
    if (delta_.write_count >= kMaxMetricPageAmount) {
      flush();
    }
  }

} // namespace storage