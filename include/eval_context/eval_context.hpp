#ifndef GRAPHDB_EVAL_CONTEXT_HPP
#define GRAPHDB_EVAL_CONTEXT_HPP
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
  size_t map_and_check(const String& key, const String& err_msg = "") const;

  void add_map(const String& key, size_t idx);

  void clear() { alias_to_slot.clear(); }

private:
  std::unordered_map<std::string, size_t> alias_to_slot;

  friend Row MergeRows(Row& first, Row& second);
};

struct Row {
  /// store current row values and names
  std::vector<RowSlot> slots;
  std::vector<String> slots_names;
  SlotMapping slots_mapping;

  void clear() {
    slots.clear();
    slots_names.clear();
    slots_mapping.clear();
  }

  Row() = default;
  Row(const Row& other) = default;

  Row(Row&& other) noexcept :
    slots(std::move(other.slots)),
    slots_names(std::move(other.slots_names)),
    slots_mapping(std::move(other.slots_mapping)) {
    other.slots.clear();
    other.slots_names.clear();
  }

  Row& operator=(const Row& other) = default;

  Row& operator=(Row&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    slots = std::move(other.slots);
    slots_names = std::move(other.slots_names);
    slots_mapping = std::move(other.slots_mapping);
    return *this;
  }

  template <typename T>
    requires std::is_constructible_v<RowSlot, T>
  void AddValue(const T& val, const String& alias, const String& error_msg = "") {
    if (slots_mapping.key_exists(alias)) {
      throw std::runtime_error(error_msg);
    }
    slots.emplace_back(val);
    slots_names.emplace_back(alias);
    slots_mapping.add_map(alias, slots.size() - 1);
  }

  ~Row() = default;
};
}

namespace ast {
struct EvalContext {
  [[nodiscard]] graph::exec::RowSlot GetAliasedObj(const std::string& alias) const {
    return row_.slots[row_.slots_mapping.map_and_check(
      alias,
      std::format("EvalContext: Error, no {} alias in slots", alias)
    )];
  }
  [[nodiscard]] Value GetProperty(const std::string& alias, const std::string& property) const {
    auto slot = GetAliasedObj(alias);
    if (std::holds_alternative<Value>(slot)) {
      throw std::runtime_error(
        std::format("EvalContext: Error, invalid expression {} has is not a node or an edge", alias)
      );
    }
    const auto& props = (std::holds_alternative<graph::exec::Edge*>(slot) ?
          std::get<graph::exec::Edge*>(slot)->properties
        : std::get<graph::exec::Node*>(slot)->properties
    );
    auto it = props.find(property);
    if (it == props.end()) {
      throw std::runtime_error(
        std::format("EvalContext: Error, alias {} has no property {}", alias, property)
      );
    }
    return it->second;
  }
  EvalContext(const graph::exec::Row& row): row_(row) {}
private:
  const graph::exec::Row& row_;
};
}  // namespace ast
#endif //GRAPHDB_EVAL_CONTEXT_HPP