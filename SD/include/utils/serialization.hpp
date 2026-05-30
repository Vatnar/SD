#pragma once
#include <VLA/Matrix.hpp>
#include <array>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace sd {

class Serializer;

class Serializable {
public:
  virtual ~Serializable() = default;

  virtual void serialize(Serializer& s) const = 0;
  virtual void deserialize(Serializer& s) = 0;
};

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
class Serializer {
public:
  explicit Serializer(std::vector<std::byte>& buffer) : m_buffer(buffer) {}

  // Write arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  void write(T value) {
    const auto bytes = reinterpret_cast<const std::byte*>(&value);
    m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(T));
  }

  // Write a C-style array
  template<typename T, usize N>
  void write(const T (&arr)[N]) {
    for (const auto& val : arr) {
      write(val);
    }
  }

  // Write std::array
  template<typename T, usize N>
  void write(const std::array<T, N>& arr) {
    for (const auto& val : arr) {
      write(val);
    }
  }

  // Write string
  void write(const std::string& value) {
    write(static_cast<u32>(value.size()));
    const auto* bytes = reinterpret_cast<const std::byte*>(&value[0]);
    m_buffer.insert(m_buffer.end(), bytes, bytes + value.size());
  }

  // Write std::vector (arithmetic types)
  template<typename T>
    requires std::is_arithmetic_v<T>
  void write(const std::vector<T>& vec) {
    write(static_cast<u32>(vec.size()));
    for (const auto& val : vec) {
      write(val);
    }
  }

  // Write raw bytes
  void write(const void* data, const usize size) {
    const auto bytes = static_cast<const std::byte*>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
  }

  // Write Serializable object
  void write(const Serializable& obj) { obj.serialize(*this); }

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
  template<typename T, usize N>
  void read(T (&arr)[N]) {
    assert(m_read_offset + sizeof(T) * N <= get_written_size());
    for (auto& val : arr) {
      val = read<T>();
    }
  }

  // Read std::array
  template<typename T, usize N>
  void read(std::array<T, N>& arr) {
    assert(m_read_offset + sizeof(T) * N <= get_written_size());
    for (auto& val : arr) {
      val = read<T>();
    }
  }

  // Read string
  std::string read_string() {
    assert(m_read_offset + sizeof(u32) <= get_written_size());
    const usize size = read<u32>();
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
    u32 size = read<u32>();
    vec.resize(size);
    for (u32 i = 0; i < size; ++i) {
      vec[i] = read<T>();
    }
  }

  // Read Serializable object
  void read(Serializable& obj) { obj.deserialize(*this); }

  // Read VLA::Matrix4x4f
  VLA::Matrix4x4f read() {
    VLA::Matrix4x4f m;
    read(m.A);
    return m;
  }

  void reset_offset() { m_read_offset = 0; }
  [[nodiscard]] usize get_offset() const { return m_read_offset; }
  void SetOffset(usize offset) { m_read_offset = offset; }

  [[nodiscard]] usize get_written_size() const { return m_buffer.size(); }
  void clear() { m_buffer.clear(); }

  [[nodiscard]] std::span<std::byte> get_span() const { return {m_buffer.data(), m_buffer.size()}; }

private:
  std::vector<std::byte>& m_buffer;
  usize m_read_offset = 0;
};

} // namespace SD
