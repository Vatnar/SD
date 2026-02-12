#pragma once
#include <expected>
#include <fstream>
#include <vector>

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

  const size_t fileSize = file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
  file.close();

  return buffer;
}
