#pragma once
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "Core/types.hpp"
#include <VLA/Matrix.hpp>

namespace SD {

class Serializer;

class Serializable {
public:
  virtual ~Serializable() = default;

  virtual void Serialize(Serializer& s) const = 0;
  virtual void Deserialize(Serializer& s) = 0;
};

class Serializer {
public:
  explicit Serializer(std::vector<u8>& buffer) : mBuffer(buffer), mOffset(0) {}

  // Write arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  void Write(T value) {
    const auto bytes = reinterpret_cast<const u8*>(&value);
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
    mBuffer.insert(mBuffer.end(), value.begin(), value.end());
  }

  // Write raw bytes
  void Write(const void* data, usize size) {
    const auto bytes = static_cast<const u8*>(data);
    mBuffer.insert(mBuffer.end(), bytes, bytes + size);
  }

  // Write Serializable object
  void Write(const Serializable& obj) { obj.Serialize(*this); }

  // Write VLA::Matrix4x4f
  void Write(const VLA::Matrix4x4f& m) {
    Write(m.A);
  }

  // Read arithmetic types
  template<typename T>
    requires std::is_arithmetic_v<T>
  T Read() {
    T value;
    std::memcpy(&value, mBuffer.data() + mOffset, sizeof(T));
    mOffset += sizeof(T);
    return value;
  }

  // Read C-style array
  template<typename T, usize N>
  void Read(T (&arr)[N]) {
    for (auto& val : arr) {
      val = Read<T>();
    }
  }

  // Read std::array
  template<typename T, usize N>
  void Read(std::array<T, N>& arr) {
    for (auto& val : arr) {
      val = Read<T>();
    }
  }

  // Read string
  std::string ReadString() {
    const usize size = Read<u32>();
    std::string str(mBuffer.begin() + static_cast<long>(mOffset),
                    mBuffer.begin() + static_cast<long>(mOffset + size));
    mOffset += size;
    return str;
  }

  // Read Serializable object
  void Read(Serializable& obj) { obj.Deserialize(*this); }

  // Read VLA::Matrix4x4f
  VLA::Matrix4x4f Read() {
    VLA::Matrix4x4f m;
    Read(m.A);
    return m;
  }

  [[nodiscard]] usize GetOffset() const { return mOffset; }
  void SetOffset(usize offset) { mOffset = offset; }

private:
  std::vector<u8>& mBuffer;
  usize mOffset = 0;
};

} // namespace SD