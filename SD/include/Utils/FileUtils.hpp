#pragma once
#include <expected>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Core/types.hpp"

namespace sd {

enum class FileError {
  NONE,
  ERROR,
  FILE_TOO_LARGE,
  ALREADY_EXISTS,
};

/**
 * Reads a file from given path
 * @param filename
 * @return array of chars
 */
inline std::expected<std::vector<char>, FileError> read_file(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return std::unexpected(FileError::ERROR);
  }

  const usize file_size = file.tellg();
  std::vector<char> buffer(file_size);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(file_size));
  file.close();

  if (!file) {
    return std::unexpected(FileError::ERROR);
  }

  return buffer;
}

namespace Filesystem {
/**
 * Overwrites a given buffer with read binary data
 * @param path
 * @param buffer * @return
 */
inline FileError read_binary(const std::filesystem::path& path, std::vector<std::byte>& buffer) {
  std::error_code ec;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    // TODO: handle errors
    engine_abort(ec.category().name());
    return FileError::ERROR;
  }
  if (file_size == std::numeric_limits<std::uintmax_t>::max()) {
    // fails to read something
    return FileError::ERROR;
  }

  if (file_size > std::numeric_limits<std::size_t>::max()) {
    return FileError::FILE_TOO_LARGE;
  }

  buffer.clear();
  buffer.resize(file_size);

  std::ifstream file(path, std::ios::binary);
  if (!file)
    return FileError::ERROR;

  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
  if (!file || file.gcount() != static_cast<std::streamsize>(file_size)) {
    return FileError::ERROR;
  }
  return FileError::NONE;
}
inline FileError write_binary(const std::filesystem::path& path, const std::vector<std::byte>& data,
                             bool overwrite_existing = false) {
  if (std::filesystem::exists(path)) {
    if (!overwrite_existing)
      return FileError::ALREADY_EXISTS;
  }
  // truncates by default
  std::ofstream file(path, std::ios::out | std::ios::binary);
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  // todo: handle error
  return FileError::NONE;
}
} // namespace Filesystem

} // namespace SD
