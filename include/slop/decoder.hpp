#pragma once

#include "slop/decoded.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace slop {
namespace decode {

// M3G bytes/path → Decoded (parse + scene build + optional pattern attach).
class Decoder {
public:
    Decoded decode_file(const std::string &input_path, const DecodeOptions &options = {}) const;
    Decoded decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                         const DecodeOptions &options = {}) const;
};

} // namespace decode
} // namespace slop
