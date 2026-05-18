#include <cassert>
#include <fstream>
#include <stdexcept>

#include "storage/metrics_store.hpp"
#include "storage/serialize.hpp"

namespace storage {

  static const std::filesystem::path kSnapshotFile = "metrics.bin";
  static const std::filesystem::path kDeltaFile = "metrics_delta.log";

  MetricsStore::MetricsStore(const std::filesystem::path& dir)
    : dir_(dir) {
    if (!dir_.empty()) load();
  }

  MetricsStore::~MetricsStore() {
    flush();
  }

  void MetricsStore::on_node_created(const std::vector<std::string>& labels,
                                     const Properties& props) {
    ++total_nodes_;
    for (const auto& label : labels) ++label_node_count_[label];
    for (const auto& [prop, val] : props) property_distinct_[prop].insert(val);

    delta_.push_back({ DeltaEvent::Type::NodeCreated, labels, props, 0 });
    maybe_flush();
  }

  void MetricsStore::on_node_deleted(const std::vector<std::string>& labels,
                                     const Properties& props) {
    assert(total_nodes_ > 0);
    --total_nodes_;
    for (const auto& label : labels) {
      auto it = label_node_count_.find(label);
      if (it != label_node_count_.end() && it->second > 0) --it->second;
    }

    delta_.push_back({ DeltaEvent::Type::NodeDeleted, labels, props, 0 });
    maybe_flush();
  }

  void MetricsStore::on_label_added(const std::string& label,
                                    const Properties& node_props,
                                    size_t out_degree) {
    ++label_node_count_[label];
    label_total_out_degree_[label] += out_degree;

    delta_.push_back({ DeltaEvent::Type::LabelAdded, {label}, node_props, out_degree });
    maybe_flush();
  }

  void MetricsStore::on_label_removed(const std::string& label, size_t out_degree) {
    auto it = label_node_count_.find(label);
    if (it != label_node_count_.end() && it->second > 0) --it->second;

    auto oit = label_total_out_degree_.find(label);
    if (oit != label_total_out_degree_.end() && oit->second >= out_degree)
      oit->second -= out_degree;

    delta_.push_back({ DeltaEvent::Type::LabelRemoved, {label}, {}, out_degree });
    maybe_flush();
  }

  void MetricsStore::on_edge_created(const std::vector<std::string>& src_labels) {
    for (const auto& label : src_labels) ++label_total_out_degree_[label];

    delta_.push_back({ DeltaEvent::Type::EdgeCreated, src_labels, {}, 0 });
    maybe_flush();
  }

  void MetricsStore::on_edge_deleted(const std::vector<std::string>& src_labels) {
    for (const auto& label : src_labels) {
      auto it = label_total_out_degree_.find(label);
      if (it != label_total_out_degree_.end() && it->second > 0) --it->second;
    }

    delta_.push_back({ DeltaEvent::Type::EdgeDeleted, src_labels, {}, 0 });
    maybe_flush();
  }

  // ── queries ───────────────────────────────────────────────────────────────

  size_t MetricsStore::node_count() const {
    return total_nodes_;
  }

  size_t MetricsStore::node_count_with_label(const std::string& label) const {
    auto it = label_node_count_.find(label);
    return it != label_node_count_.end() ? it->second : 0;
  }

  std::optional<size_t> MetricsStore::property_distinct_count(
    const std::string& prop, const std::string& label) const {
    // label фильтрация не поддерживается без LPV — возвращаем глобальный счётчик
    auto it = property_distinct_.find(prop);
    return it != property_distinct_.end() ? it->second.size() : 0;
  }

  double MetricsStore::avg_out_degree(const std::string& label) const {
    auto cnt_it = label_node_count_.find(label);
    if (cnt_it == label_node_count_.end() || cnt_it->second == 0) return 0.0;
    auto deg_it = label_total_out_degree_.find(label);
    if (deg_it == label_total_out_degree_.end()) return 0.0;
    return static_cast<double>(deg_it->second) / cnt_it->second;
  }

  // ── persistence ───────────────────────────────────────────────────────────

  void MetricsStore::flush() {
    if (delta_.empty()) return;
    if (!dir_.empty()) persist_delta();
    delta_.clear();
    write_count_ = 0;
  }

  void MetricsStore::clear() {
    total_nodes_ = 0;
    label_node_count_.clear();
    label_total_out_degree_.clear();
    property_distinct_.clear();
    delta_.clear();
    write_count_ = 0;
  }

  void MetricsStore::maybe_flush() {
    if (++write_count_ >= kMaxMetricPageAmount) flush();
  }

  void MetricsStore::persist_delta() {
    std::ofstream os(dir_ / kDeltaFile, std::ios::binary | std::ios::app);
    if (!os) return;

    for (const auto& e : delta_) {
      serial::write<uint8_t>(os, static_cast<uint8_t>(e.type));

      serial::write<uint32_t>(os, static_cast<uint32_t>(e.labels.size()));
      for (const auto& l : e.labels) serial::write_str(os, l);

      serial::write_properties(os, e.props);
      serial::write<uint64_t>(os, static_cast<uint64_t>(e.out_degree));
    }
  }

  void MetricsStore::write_snapshot() {
    if (dir_.empty()) return;
    std::ofstream os(dir_ / kSnapshotFile, std::ios::binary | std::ios::trunc);
    if (!os) return;

    serial::write<uint64_t>(os, total_nodes_);

    serial::write<uint32_t>(os, static_cast<uint32_t>(label_node_count_.size()));
    for (const auto& [label, count] : label_node_count_) {
      serial::write_str(os, label);
      serial::write<uint64_t>(os, count);
    }

    serial::write<uint32_t>(os, static_cast<uint32_t>(label_total_out_degree_.size()));
    for (const auto& [label, deg] : label_total_out_degree_) {
      serial::write_str(os, label);
      serial::write<uint64_t>(os, deg);
    }

    serial::write<uint32_t>(os, static_cast<uint32_t>(property_distinct_.size()));
    for (const auto& [prop, vals] : property_distinct_) {
      serial::write_str(os, prop);
      serial::write<uint32_t>(os, static_cast<uint32_t>(vals.size()));
      for (const auto& v : vals) serial::write_value(os, v);
    }

    // снапшот записан — можно удалить старый лог
    std::filesystem::remove(dir_ / kDeltaFile);
  }

  void MetricsStore::replay_log() {
    std::ifstream is(dir_ / kDeltaFile, std::ios::binary);
    if (!is) return;

    while (is.peek() != EOF) {
      auto type = static_cast<DeltaEvent::Type>(serial::read<uint8_t>(is));

      uint32_t label_count = serial::read<uint32_t>(is);
      std::vector<std::string> labels(label_count);
      for (auto& l : labels) l = serial::read_str(is);

      Properties props = serial::read_properties(is);
      size_t out_degree = static_cast<size_t>(serial::read<uint64_t>(is));

      switch (type) {
        case DeltaEvent::Type::NodeCreated:
          ++total_nodes_;
          for (const auto& label : labels) ++label_node_count_[label];
          for (const auto& [prop, val] : props) property_distinct_[prop].insert(val);
          break;
        case DeltaEvent::Type::NodeDeleted:
          if (total_nodes_ > 0) --total_nodes_;
          for (const auto& label : labels) {
            auto it = label_node_count_.find(label);
            if (it != label_node_count_.end() && it->second > 0) --it->second;
          }
          break;
        case DeltaEvent::Type::LabelAdded:
          ++label_node_count_[labels[0]];
          label_total_out_degree_[labels[0]] += out_degree;
          break;
        case DeltaEvent::Type::LabelRemoved:
        {
          auto it = label_node_count_.find(labels[0]);
          if (it != label_node_count_.end() && it->second > 0) --it->second;
          auto oit = label_total_out_degree_.find(labels[0]);
          if (oit != label_total_out_degree_.end() && oit->second >= out_degree)
            oit->second -= out_degree;
        }
          break;
        case DeltaEvent::Type::EdgeCreated:
          for (const auto& label : labels) ++label_total_out_degree_[label];
          break;
        case DeltaEvent::Type::EdgeDeleted:
          for (const auto& label : labels) {
            auto it = label_total_out_degree_.find(label);
            if (it != label_total_out_degree_.end() && it->second > 0) --it->second;
          }
          break;
      }
    }
  }

  void MetricsStore::load() {
    // читаем снапшот
    std::ifstream is(dir_ / kSnapshotFile, std::ios::binary);
    if (is) {
      total_nodes_ = static_cast<size_t>(serial::read<uint64_t>(is));

      uint32_t n = serial::read<uint32_t>(is);
      for (uint32_t i = 0; i < n; ++i) {
        auto label = serial::read_str(is);
        label_node_count_[label] = static_cast<size_t>(serial::read<uint64_t>(is));
      }

      n = serial::read<uint32_t>(is);
      for (uint32_t i = 0; i < n; ++i) {
        auto label = serial::read_str(is);
        label_total_out_degree_[label] = static_cast<size_t>(serial::read<uint64_t>(is));
      }

      n = serial::read<uint32_t>(is);
      for (uint32_t i = 0; i < n; ++i) {
        auto prop = serial::read_str(is);
        uint32_t val_count = serial::read<uint32_t>(is);
        for (uint32_t j = 0; j < val_count; ++j)
          property_distinct_[prop].insert(serial::read_value(is));
      }
    }

    replay_log();
  }

} // namespace storage