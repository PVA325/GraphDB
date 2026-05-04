#ifndef GRAPHDB_EXEC_CONTEXT_HPP
#define GRAPHDB_EXEC_CONTEXT_HPP

#include "common/common_value.hpp"

namespace graph::exec {
/// Operations need to live longer than cursors
struct ExecOptions {
  /// options of execution; memory, in future parallelism and spill
  size_t memory_budget_bytes = 256ULL * 1024 * 1024;
};

struct ExecContext {
  /// Context of execution db
  storage::GraphDB* db;
  ExecOptions options;

  explicit ExecContext() : db{nullptr} {
  }

  ExecContext(storage::GraphDB* db) : db(db) {
  }
};
}

#endif //GRAPHDB_EXEC_CONTEXT_HPP
