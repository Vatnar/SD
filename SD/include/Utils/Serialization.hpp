#pragma once
#include <VLA/Matrix.hpp>
#include <array>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "Core/types.hpp"

namespace SD {

class Serializer;

class Serializable {
public:
  virtual ~Serializable() = default;

  virtual void Serialize(Serializer& s) const = 0;
  virtual void Deserialize(Serializer& s) = 0;
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
  explicit Serializer(std::vector<std::byte>& buffer) : mBuffer(buffer) {}

  // Write arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  void Write(T value) {
    const auto bytes = reinterpret_cast<const std::byte*>(&value);
    mBuffer.insert(mBuffer.end(), bytes, bytes + sizeof(T));
  }

  // Write a C-style array
  template<typename T, usize N>
  void Write(const T (&arr)[N]) {
    for (const auto& val : arr) {
      Write(val);
    }
  }

  // Write std::array
  template<typename T, usize N>
  void Write(const std::array<T, N>& arr) {
    for (const auto& val : arr) {
      Write(val);
    }
  }

  // Write string
  void Write(const std::string& value) {
    Write(static_cast<u32>(value.size()));
    const auto* bytes = reinterpret_cast<const std::byte*>(&value[0]);
    mBuffer.insert(mBuffer.end(), bytes, bytes + value.size());
  }

  // Write std::vector (arithmetic types)
  template<typename T>
    requires std::is_arithmetic_v<T>
  void Write(const std::vector<T>& vec) {
    Write(static_cast<u32>(vec.size()));
    for (const auto& val : vec) {
      Write(val);
    }
  }

  // Write raw bytes
  void Write(const void* data, const usize size) const {
    const auto bytes = static_cast<const std::byte*>(data);
    mBuffer.insert(mBuffer.end(), bytes, bytes + size);
  }

  // Write Serializable object
  void Write(const Serializable& obj) { obj.Serialize(*this); }

  // Write VLA::Matrix4x4f
  void Write(const VLA::Matrix4x4f& m) { Write(m.A); }

  // Read arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  T Read() {
    assert(mReadOffset + sizeof(T) <= GetWrittenSize());
    T value;
    std::memcpy(&value, mBuffer.data() + mReadOffset, sizeof(T));
    mReadOffset += sizeof(T);
    return value;
  }

  // Read C-style array
  template<typename T, usize N>
  void Read(T (&arr)[N]) {
    assert(mReadOffset + sizeof(T) * N <= GetWrittenSize());
    for (auto& val : arr) {
      val = Read<T>();
    }
  }

  // Read std::array
  template<typename T, usize N>
  void Read(std::array<T, N>& arr) {
    assert(mReadOffset + sizeof(T) * N <= GetWrittenSize());
    for (auto& val : arr) {
      val = Read<T>();
    }
  }

  // Read string
  std::string ReadString() {
    assert(mReadOffset + sizeof(u32) <= GetWrittenSize());
    const usize size = Read<u32>();
    assert(mReadOffset + size * sizeof(char) <= GetWrittenSize());
    const char* ptr = reinterpret_cast<const char*>(mBuffer.data() + mReadOffset);
    std::string str(ptr, size);
    mReadOffset += size;
    return str;
  }

  // Read std::vector (arithmetic types)
  template<typename T>
    requires std::is_arithmetic_v<T>
  void Read(std::vector<T>& vec) {
    u32 size = Read<u32>();
    vec.resize(size);
    for (u32 i = 0; i < size; ++i) {
      vec[i] = Read<T>();
    }
  }

  // Read Serializable object
  // TODO: this doesnt increment the read offset
  void Read(Serializable& obj) { obj.Deserialize(*this); }

  // Read VLA::Matrix4x4f
  VLA::Matrix4x4f Read() {
    VLA::Matrix4x4f m;
    Read(m.A);
    return m;
  }

  void ResetOffset() { mReadOffset = 0; }
  [[nodiscard]] usize GetOffset() const { return mReadOffset; }
  void SetOffset(usize offset) { mReadOffset = offset; }

  [[nodiscard]] usize GetWrittenSize() const { return mBuffer.size(); }
  void Clear() const { mBuffer.clear(); }

  [[nodiscard]] std::span<std::byte> GetSpan() const { return {mBuffer.data(), mBuffer.size()}; }

private:
  std::vector<std::byte>& mBuffer;
  usize mReadOffset = 0;
};

} // namespace SD
