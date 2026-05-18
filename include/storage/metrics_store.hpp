#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "types.hpp"

namespace storage {

  class MetricsStore {
  public:
    static constexpr size_t kMaxMetricPageAmount = 1024;

    explicit MetricsStore(const std::filesystem::path& dir);
    ~MetricsStore();

    void on_node_created(const std::vector<std::string>& labels, const Properties& props);
    void on_node_deleted(const std::vector<std::string>& labels, const Properties& props);

    void on_label_added(const std::string& label, const Properties& node_props, size_t out_degree);
    void on_label_removed(const std::string& label, size_t out_degree);

    void on_edge_created(const std::vector<std::string>& src_labels);
    void on_edge_deleted(const std::vector<std::string>& src_labels);

    size_t node_count() const;
    size_t node_count_with_label(const std::string& label) const;
    std::optional<size_t> property_distinct_count(const std::string& prop,
                                                  const std::string& label) const;
    double avg_out_degree(const std::string& label) const;

    void flush();
    void clear();

  private:
    const std::filesystem::path dir_;

    size_t total_nodes_ = 0;
    std::unordered_map<std::string, size_t> label_node_count_;
    std::unordered_map<std::string, size_t> label_total_out_degree_;
    std::unordered_map<std::string, std::unordered_set<Value>> property_distinct_;

    std::vector<DeltaEvent> delta_;
    size_t write_count_ = 0;

    void persist_delta();
    void maybe_flush();
    void load();
    void write_snapshot();
    void replay_log();
  };

} // namespace storage