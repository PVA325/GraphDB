#pragma once

#include "common/common_value.hpp"

namespace graph::exec {

struct RowCursor {
  /// iterator by Row
  /// used to obtain next row(at the end need to be closed)
  RowCursor() = default;

  virtual bool next(Row& out) = 0;

  virtual void close() = 0;

  virtual ~RowCursor() = default;
};

} // namespace graph::exec

