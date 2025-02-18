#pragma once

#include <filesystem>
#include <string>
#include <fstream>

#if __cplusplus < 202002L
# error Should be at least C++20
#endif

namespace fs = std::filesystem;

namespace raf
{

inline std::string read_all_file(const fs::path& in_file)
{
  std::ifstream ifs{in_file};
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

}