#ifndef M3G_GLTF_WRITER_HPP_INCLUDED
#define M3G_GLTF_WRITER_HPP_INCLUDED

#include "m3g.hpp"

#include <string>

namespace m3g {
namespace gltf {

class GltfWriter {
public:
    scene::GltfWriteResult write(const scene::SceneIr &scene, const std::string &output_path, bool overwrite,
                                 int png_compression_level = 8);
};

} // namespace gltf
} // namespace m3g

#endif /* M3G_GLTF_WRITER_HPP_INCLUDED */
