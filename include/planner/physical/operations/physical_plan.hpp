#pragma once

#include "physical_op_no_child.hpp"

namespace graph::exec {
struct PhysicalPlan {
  /// creates physical plan
  PhysicalOpPtr root;

  PhysicalPlan() = default;

  PhysicalPlan(PhysicalPlan&& other) noexcept : root(std::move(other.root)) {}

  PhysicalPlan& operator=(PhysicalPlan&& other) noexcept {
    root = std::move(other.root);
    return *this;
  }

  explicit PhysicalPlan(PhysicalOpPtr root): root(std::move(root)) {}

  [[nodiscard]] std::string DebugString() const;
};

}
