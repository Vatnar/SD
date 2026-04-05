#pragma once
#include <expected>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Core/types.hpp"

namespace SD {
/**
 * Reads a file from given path
 * @param filename
 * @return array of chars
 */
inline std::expected<std::vector<char>, std::unexpect_t> readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return std::unexpected(std::unexpect);
  }

  const usize fileSize = file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
  file.close();

  return buffer;
}

namespace Filesystem {
enum class FileError {
  none,
  error,
  file_too_large,
  already_exists,
};
/**
 * Overwrites given buffer with read binary data
 * @param path
 * @param buffer * @return
 */
inline FileError ReadBinary(const std::filesystem::path& path, std::vector<std::byte>& buffer) {
  std::error_code ec;
  const auto fileSize = std::filesystem::file_size(path, ec);
  if (ec) {
    // TODO: handle errors
    SD::Abort(ec.category().name());
    return FileError::error;
  }
  if (fileSize == std::numeric_limits<std::uintmax_t>::max()) {
    // fails to read something
    return FileError::error;
  }

  if (fileSize > std::numeric_limits<std::size_t>::max()) {
    return FileError::file_too_large;
  }

  buffer.clear();
  buffer.resize(fileSize);

  std::ifstream file(path, std::ios::binary);
  if (!file)
    return FileError::error;

  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
  if (!file || file.gcount() != static_cast<std::streamsize>(fileSize)) {
    return FileError::error;
  }
  return FileError::none;
}
inline FileError WriteBinary(const std::filesystem::path& path, const std::vector<std::byte>& data,
                             bool overwriteExisting = false) {
  if (std::filesystem::exists(path)) {
    if (!overwriteExisting)
      return FileError::already_exists;
  }
  // truncates by default
  std::ofstream file(path, std::ios::out | std::ios::binary);
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  // todo: handle error
  return FileError::none;
}
} // namespace Filesystem

} // namespace SD
