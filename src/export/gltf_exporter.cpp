#include "slop/gltf_exporter.hpp"

#include "gltf/gltf_writer.hpp"

namespace slop {
namespace exp {

GltfPaths GltfExporter::write(const decode::Decoded &decoded, const std::string &output_path, bool overwrite) const {
    return write(decoded.scene_ir, output_path, overwrite);
}

GltfPaths GltfExporter::write(const scene::SceneIr &scene_ir, const std::string &output_path, bool overwrite) const {
    gltf::GltfWriter writer;
    auto result = writer.write(scene_ir, output_path, overwrite);
    GltfPaths paths;
    paths.gltf_path = std::move(result.gltf_path);
    paths.bin_path = std::move(result.bin_path);
    paths.image_paths = std::move(result.image_paths);
    return paths;
}

} // namespace exp
} // namespace slop
