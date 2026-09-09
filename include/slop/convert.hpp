#pragma once

/*
 * Public convert / export API for slop (C++17).
 *
 *   #include <slop/convert.hpp>
 *
 * Full pipeline:
 *   slop::M3gConverter conv;
 *   auto report = conv.convert("in.m3g", "out.gltf", true);
 *
 * Or staged:
 *   auto decoded = conv.decode("in.m3g");
 *   auto paths = conv.export_gltf(decoded, "out.gltf", true);
 *
 * Low-level exporter:
 *   slop::exp::GltfExporter{}.write(decoded, "out.gltf", true);
 */

#include "slop/converter.hpp"
#include "slop/export_result.hpp"
#include "slop/gltf_exporter.hpp"
