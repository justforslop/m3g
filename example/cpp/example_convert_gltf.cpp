// test_convert_gltf.cpp
#define M3G_IMPL
#include "m3g.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char **argv) {
    const std::string input  = (argc > 1) ? argv[1] : "simple.m3g";
    const std::string output = (argc > 2) ? argv[2] : "out.gltf";

    // --- install all backends before load (same idea as sokol/stb adapters) --
    m3g::install_miniz_deflate_io();
    m3g::install_stb_image_io();
    m3g::install_cjson_json_io();
    m3g::install_cgltf_gltf_io();

    assert(m3g::deflate_io() != nullptr);
    assert(m3g::image_io()   != nullptr);
    assert(m3g::json_io()    != nullptr);
    assert(m3g::gltf_io()    != nullptr);

    if (!fs::exists(input)) {
        std::cerr << "skip: " << input << " not found\n";
        return 0;   // treat missing fixture as skip, not failure
    }
    std::error_code ec;
    fs::remove(output, ec);

    m3g::Converter cvt;
    m3g::exp::ExportReport rep;
    try {
        rep = cvt.convert(input, output, /*overwrite=*/true);
    } catch (const std::exception &e) {
        std::cerr << "convert failed: " << e.what() << "\n";
        return 1;
    }


    // Every emitted image path must exist on disk.
    for (const auto &img : rep.paths.image_paths)
        assert(fs::exists(img));

    // The decoded graph must survive the conversion.
    assert(!rep.decoded.objects_by_id.empty() || rep.decoded.world);

    // --- JSON structure sanity (grep-level, no JSON parser dependency) ------
    std::ifstream in(rep.paths.gltf_path);
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    assert(text.find("\"asset\"")  != std::string::npos);
    assert(text.find("\"scenes\"") != std::string::npos);

    // --- scene IR must not contain dangling indices -------------------------
    const auto &ir = rep.decoded.scene_ir;
    for (const auto &mesh : ir.meshes)
        for (const auto &prim : mesh.primitives)
            if (prim.material_index)
                assert(*prim.material_index >= 0 &&
                       *prim.material_index < static_cast<int>(ir.materials.size()));
    for (const auto &tex : ir.textures)
        assert(tex.image_index >= 0 &&
               tex.image_index < static_cast<int>(ir.images.size()));

    std::cout << "test_convert_gltf: OK -> " << rep.paths.gltf_path << "\n";
    return 0;
}
