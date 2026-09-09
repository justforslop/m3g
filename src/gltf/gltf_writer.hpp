#pragma once

#include "slop/scene_model.hpp"

#include <string>

namespace slop {
namespace gltf {

class GltfWriter {
public:
    scene::GltfWriteResult write(const scene::SceneIr &scene, const std::string &output_path, bool overwrite);
};

} // namespace gltf
} // namespace slop
