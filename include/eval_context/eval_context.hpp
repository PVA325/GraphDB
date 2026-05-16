#pragma once

#include <format>
#include <variant>
#include "common/common_value.hpp"

namespace graph::exec {
struct SlotMapping {
  SlotMapping() = default;
  SlotMapping(const SlotMapping&) = default;

  SlotMapping(SlotMapping&& other) noexcept : alias_to_slot(std::move(other.alias_to_slot)) {
    other.alias_to_slot.clear();
  }

  SlotMapping& operator=(const SlotMapping&) = default;

  SlotMapping& operator=(SlotMapping&& other) noexcept {
    if (this != &other) {
      alias_to_slot = std::move(other.alias_to_slot);
    }
    return *this;
  }

  ~SlotMapping() = default;

  bool key_exists(const std::string& key) const;

  size_t map(const std::string& key) const;
  size_t map_and_check(const String& key, std::string_view err_msg = "") const;

  void add_map(const String& key, size_t idx);

  void clear() { alias_to_slot.clear(); }

private:
  std::unordered_map<std::string, size_t> alias_to_slot;

  friend std::tuple<Row, bool> MergeRows(const Row& first, const Row& second);
};

struct Row {
  /// store current row values and names
  void clear() {
    slots_.clear();
    slots_names_.clear();
    slots_mapping_.clear();
  }

  Row() = default;
  Row(const Row& other) = default;

  Row(Row&& other) noexcept :
    slots_(std::move(other.slots_)),
    slots_names_(std::move(other.slots_names_)),
    slots_mapping_(std::move(other.slots_mapping_)) {
  }

  Row& operator=(const Row& other) = default;

  Row& operator=(Row&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    slots_ = std::move(other.slots_);
    slots_names_ = std::move(other.slots_names_);
    slots_mapping_ = std::move(other.slots_mapping_);
    return *this;
  }

  template <typename T>
    requires std::is_constructible_v<RowSlot, T>
  void AddSlot(T&& val, const String& alias, const String& error_msg = "") {
    if (alias == "") return;
    if (slots_mapping_.key_exists(alias)) {
      throw std::runtime_error(error_msg);
    }
    slots_.emplace_back(std::forward<T>(val));
    slots_names_.emplace_back(alias);
    slots_mapping_.add_map(alias, slots_.size() - 1);
  }

  [[nodiscard]] graph::exec::RowSlot GetAliasedObj(const std::string& alias, std::string_view alias_not_found_err = "") const {
    return slots_[slots_mapping_.map_and_check(
      alias,
      (alias_not_found_err.empty() ? std::format("EvalContext: Error, no {} alias in slots", alias) : alias_not_found_err)
    )];
  }

  Value GetProperty(const std::string& alias, const std::string& property, const std::string& parent_op = "") const;
  bool KeyExists(const String& key) const { return slots_mapping_.key_exists(key); }
  const std::vector<RowSlot> Slots() const { return slots_; }
  const std::vector<String> SlotsNames() const { return slots_names_; }
  ~Row() = default;
private:
  std::vector<RowSlot> slots_;
  std::vector<String> slots_names_;
  SlotMapping slots_mapping_;

  friend std::tuple<Row, bool> MergeRows(const Row& first, const Row& second);
};
}

namespace ast {
struct EvalContext {
  graph::exec::RowSlot GetAliasedObj(const std::string& alias) const {
    return row_.GetAliasedObj(alias);
  }

  Value GetProperty(const std::string& alias, const std::string& property) const {
    return row_.GetProperty(alias, property);
  }

  EvalContext(const graph::exec::Row& row) : row_(row) {
  }

private:
  const graph::exec::Row& row_;
};
} // namespace ast
