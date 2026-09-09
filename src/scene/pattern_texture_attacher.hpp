#pragma once

#include "slop/scene_model.hpp"

#include <optional>
#include <string>

namespace slop {
namespace scene {

struct PatternTextureAttacher {
    static SceneIr auto_attach(const SceneIr &scene, const std::optional<std::string> &pattern_path);
};

} // namespace scene
} // namespace slop
