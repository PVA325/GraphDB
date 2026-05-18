#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>

#include "storage/TypesStructures/types.hpp"


namespace storage::serial {
  template<typename T>
  void write(std::ostream& os, T v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(T));
  }

  template<>
  inline void write<bool>(std::ostream& os, bool v) {
    uint8_t u = v ? 1 : 0;
    os.write(reinterpret_cast<const char*>(&u), 1);
  }

  inline void write_str(std::ostream& os, const std::string& s) {
    write<uint32_t>(os, static_cast<uint32_t>(s.size()));
    os.write(s.data(), s.size());
  }

  inline void write_value(std::ostream& os, const Value& val) {
    write<uint8_t>(os, static_cast<uint8_t>(val.index()));
    switch (val.index()) {
      case 0: write<int64_t>(os, std::get<int64_t>(val)); break;
      case 1: write<double> (os, std::get<double>(val)); break;
      case 2: write_str (os, std::get<std::string>(val)); break;
      case 3: write<bool> (os, std::get<bool>(val)); break;
    }
  }

  inline void write_properties(std::ostream& os, const Properties& props) {
    write<uint32_t>(os, static_cast<uint32_t>(props.size()));
    for (const auto& [k, v] : props) {
      write_str  (os, k);
      write_value(os, v);
    }
  }

  template<typename T>
  T read(std::istream& is) {
    T v;
    is.read(reinterpret_cast<char*>(&v), sizeof(T));
    return v;
  }


  template<>
  inline bool read<bool>(std::istream& is) {
    return read<uint8_t>(is) != 0;
  }

  inline std::string read_str(std::istream& is) {
    uint32_t len = read<uint32_t>(is);
    std::string s(len, '\0');
    is.read(s.data(), len);
    return s;
  }

  inline Value read_value(std::istream& is) {
    switch (read<uint8_t>(is)) {
      case 0: return read<int64_t>(is);
      case 1: return read<double> (is);
      case 2: return read_str     (is);
      case 3: return read<bool>   (is);
      default: throw std::runtime_error("unknown value type");
    }
  }

  inline Properties read_properties(std::istream& is) {
    Properties props;
    uint32_t count = read<uint32_t>(is);
    props.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      std::string key = read_str(is);
      props[std::move(key)] = read_value(is);
    }
    return props;
  }

} // namespace storage::serial