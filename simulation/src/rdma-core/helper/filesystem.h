#pragma once

#include <filesystem>
#include <string>

#if __cplusplus < 202002L
#   error C++ standard should be at least C++20
#endif

// It's pretty common to wrap `std::filesystem` in `fs`.
namespace fs = std::filesystem;

namespace ns3 {

//! Reads completely a file and stores it into a string.
std::string read_all_file(const fs::path& in_file);

} // namespace ns3