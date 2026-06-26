#pragma once
#include <array>
#include <concepts>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include <VLA/Matrix.hpp>

#include "SD/core/types.hpp"

namespace sd {

struct Serializer;

// ADL-based serialization detection
template<typename T>
concept HasSerialize = requires(Serializer& s, const T& obj) { serialize(s, obj); };

template<typename T>
concept HasDeserialize = requires(Serializer& s, T& obj) { deserialize(s, obj); };

template<typename T> concept HasSerialization = HasSerialize<T> && HasDeserialize<T>;

/**
 * Writes and reads data to a byte vector
 * Offset from start can be set with SetOffset()
 *
 * To read from beginning:
 * \code{.cpp}
 * Serializer s(buffer);
 * s.ResetOffset();
 * // Or equivalently
 * s.SetOffset(0);
 * \endcode
 */
struct Serializer {
  explicit Serializer(std::vector<std::byte>& buffer) : m_buffer(buffer) {}

  // Write arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  void write(const T& value) {
    const auto bytes = reinterpret_cast<const std::byte*>(&value);
    m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(T));
  }

  // Write a C-style array
  template<typename T, USize N>
  void write(const T (&arr)[N]) {
    for (const auto& val : arr) {
      write(val);
    }
  }

  // Write std::array
  template<typename T, USize N>
  void write(const std::array<T, N>& arr) {
    for (const auto& val : arr) {
      write(val);
    }
  }

  // Write string
  void write(const std::string& value) {
    write(static_cast<U32>(value.size()));
    const auto* bytes = reinterpret_cast<const std::byte*>(&value[0]);
    m_buffer.insert(m_buffer.end(), bytes, bytes + value.size());
  }

  // Write std::vector (arithmetic types)
  template<typename T>
    requires std::is_arithmetic_v<T>
  void write(const std::vector<T>& vec) {
    write(static_cast<U32>(vec.size()));
    for (const auto& val : vec) {
      write(val);
    }
  }

  // Write raw bytes
  void write(const void* data, const USize size) {
    const auto bytes = static_cast<const std::byte*>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
  }

  // Write ADL-serializable object
  template<HasSerialize T>
  void write(const T& obj) {
    serialize(*this, obj);
  }

  // Write VLA::Matrix4x4f
  void write(const VLA::Matrix4x4f& m) { write(m.A); }

  // Read arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  T read() {
    assert(m_read_offset + sizeof(T) <= get_written_size());
    T value;
    std::memcpy(&value, m_buffer.data() + m_read_offset, sizeof(T));
    m_read_offset += sizeof(T);
    return value;
  }

  // Read C-style array
  template<typename T, USize N>
  void read(T (&arr)[N]) {
    assert(m_read_offset + sizeof(T) * N <= get_written_size());
    for (auto& val : arr) {
      val = read<T>();
    }
  }

  // Read std::array
  template<typename T, USize N>
  void read(std::array<T, N>& arr) {
    assert(m_read_offset + sizeof(T) * N <= get_written_size());
    for (auto& val : arr) {
      val = read<T>();
    }
  }

  // Read string
  std::string read_string() {
    assert(m_read_offset + sizeof(U32) <= get_written_size());
    const USize size = read<U32>();
    assert(m_read_offset + size * sizeof(char) <= get_written_size());
    const char* ptr = reinterpret_cast<const char*>(m_buffer.data() + m_read_offset);
    std::string str(ptr, size);
    m_read_offset += size;
    return str;
  }

  // Read std::vector (arithmetic types)
  template<typename T>
    requires std::is_arithmetic_v<T>
  void read(std::vector<T>& vec) {
    U32 size = read<U32>();
    vec.resize(size);
    for (U32 i = 0; i < size; ++i) {
      vec[i] = read<T>();
    }
  }

  // Read ADL-deserializable object
  template<HasDeserialize T>
  void read(T& obj) {
    deserialize(*this, obj);
  }

  // Read VLA::Matrix4x4f
  VLA::Matrix4x4f read() {
    VLA::Matrix4x4f m;
    read(m.A);
    return m;
  }

  void                reset_offset() { m_read_offset = 0; }
  [[nodiscard]] USize get_offset() const { return m_read_offset; }
  void                SetOffset(USize offset) { m_read_offset = offset; }

  [[nodiscard]] USize get_written_size() const { return m_buffer.size(); }
  void                clear() { m_buffer.clear(); }

  [[nodiscard]] std::span<std::byte> get_span() const { return {m_buffer.data(), m_buffer.size()}; }


  std::vector<std::byte>& m_buffer;
  USize                   m_read_offset = 0;
};

} // namespace sd
