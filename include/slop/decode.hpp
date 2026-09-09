#pragma once

/*
 * Public decode API for slop (C++17).
 *
 *   #include <slop/decode.hpp>
 *
 *   slop::decode::Decoder decoder;
 *   slop::decode::DecodeOptions opt;
 *   opt.pattern_path = "pattern.png";   // optional
 *   slop::decode::Decoded decoded = decoder.decode_file("model.m3g", opt);
 *
 *   // decoded.m3g       — raw M3G object graph
 *   // decoded.scene_ir  — export-ready intermediate scene
 *   // decoded.warnings()
 */

#include "slop/decoded.hpp"
#include "slop/decoder.hpp"
