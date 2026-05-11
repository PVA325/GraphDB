#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "string_interner.hpp"
#include "types.hpp"

namespace storage {
  struct LPVKey {
    uint32_t label_id;
    uint32_t prop_id;
    Value value;
    bool operator==(const LPVKey& item) const {
      return label_id == item.label_id && prop_id == item.prop_id && value == item.value;
    }
  };
  struct LPVKeyHash { size_t operator()(const LPVKey& k) const; };

  struct LPKey {
    uint32_t label_id;
    uint32_t prop_id;
    bool operator==(const LPKey& o) const {
      return label_id == o.label_id && prop_id == o.prop_id;
    }
  };

  struct LPKeyHash {
    size_t operator()(const LPKey& key) const {
      return std::hash<uint64_t>{}((uint64_t)key.label_id << 32 | key.prop_id);
    }
  };

  class MetricsStore {
  public:
    static constexpr size_t kMaxMetricPageAmount = 1024;

    explicit MetricsStore(const std::string& dir);
    ~MetricsStore();

    void on_node_created (const std::vector<std::string>& labels, const Properties& props);
    void on_node_deleted (const std::vector<std::string>& labels, const Properties& props);

    void on_label_added  (const std::string& label, const Properties& node_props, size_t out_degree);
    void on_label_removed(const std::string& label, size_t out_degree);

    void on_edge_created(const std::vector<std::string>& src_labels);
    void on_edge_deleted(const std::vector<std::string>& src_labels);

    size_t node_count() const;
    size_t node_count_with_label  (const std::string& label) const;
    std::optional<size_t> property_distinct_count(const std::string& prop,
                                                  const std::string& label) const;
    double avg_out_degree(const std::string& label) const;
    bool has_property_index(const std::string& label,
                              const std::string& prop) const;
    size_t property_count(const std::string& prop, const Value& value,
                          const std::string& label) const;

    void flush();
    void clear();

  private:
    std::string dir_;
    StringInterner label_interner_;
    StringInterner prop_interner_;

    size_t total_nodes_ = 0;
    std::unordered_map<std::string, size_t> label_node_count_;
    std::unordered_map<std::string, size_t> label_total_out_degree_;

    std::unordered_map<std::string, std::unordered_set<Value>> property_distinct_;

    std::unordered_map<LPVKey, size_t, LPVKeyHash> lpv_count_;
    std::unordered_map<LPKey, std::unordered_set<Value>, LPKeyHash> lp_distinct_;

    struct Delta {
      std::unordered_map<LPVKey, int32_t, LPVKeyHash> count_delta;
      std::unordered_map<LPKey, std::unordered_set<Value>, LPKeyHash> new_distinct;
      size_t write_count = 0;
      bool empty() const { return write_count == 0; }
      void clear() { count_delta.clear(); new_distinct.clear(); write_count = 0; }
    } delta_;

    LPVKey make_lpv(const std::string& label, const std::string& prop, const Value& val);
    LPKey make_lp(const std::string& label, const std::string& prop);

    void apply_delta();
    void persist_delta();
    void maybe_flush();
  };
} // namespace storage