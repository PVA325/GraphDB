#ifndef GRAPHDB_ROW_CURSOR_HPP
#define GRAPHDB_ROW_CURSOR_HPP

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

}

#endif //GRAPHDB_ROW_CURSOR_HPP