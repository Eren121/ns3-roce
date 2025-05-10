#include "ns3/filesystem.h"
#include <fstream>

namespace ns3 {

std::string read_all_file(const fs::path& in_file)
{
    std::ifstream ifs{in_file};
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace ns3