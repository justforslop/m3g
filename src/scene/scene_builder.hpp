#pragma once

#include "slop/m3g_model.hpp"
#include "slop/scene_model.hpp"

#include <string>

namespace slop {
namespace scene {

class M3gSceneBuilder {
public:
    M3gSceneBuilder(const m3g::File &file, std::string input_path);

    SceneIr build();

private:
    const m3g::File &file_;
    std::string input_path_;
};

} // namespace scene
} // namespace slop
