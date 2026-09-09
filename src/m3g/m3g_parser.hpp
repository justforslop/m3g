#pragma once

#include "slop/m3g_model.hpp"

#include <string>
#include <vector>

namespace slop {
namespace m3g {

class Parser {
public:
    File parse_path(const std::string &path) const;
    File parse(const std::vector<std::uint8_t> &bytes) const;
};

} // namespace m3g
} // namespace slop
