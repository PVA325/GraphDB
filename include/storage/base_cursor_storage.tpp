namespace storage {
  template<typename T, typename Id>
  bool Cursor<T, Id>::next(T *&out) {
    while (index_ < ids_.size()) {
      if (limit_ && limit_ <= returned_) {
        return false;
      }

      Id id = ids_[index_++];
      T *obj = get_from_db(id);

      if (!obj || !obj->alive) {
        continue;
      }

      if (predicate_(obj)) {
        continue;
      }

      out = obj;
      ++returned_;
      return true;
    }
    return false;
  }
} // namespace