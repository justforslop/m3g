// test_decode_walk.cpp
#define M3G_IMPL
#include "m3g.hpp"

#include <cassert>
#include <iostream>

int main() {
    m3g::install_miniz_deflate_io();

    m3g::decode::Decoded d;
    try {
        d = m3g::Loader::load("simple.m3g");
    } catch (const std::exception &e) {
        std::cerr << "load failed: " << e.what() << "\n";
        return 1;
    }

    // --- object graph checks -------------------------------------------------
    assert(!d.objects_by_id.empty());
    assert(d.objects_by_id[0] == nullptr);          // id 0 is unused by spec

    if (auto w = d.world) {
        // A valid World may legitimately have 0 children, but must be reachable.
        std::cout << "World children = " << w->getChildCount() << "\n";
        if (auto cam = w->getActiveCamera())
            std::cout << "active camera = " << cam->getClassName() << "\n";
    } else {
        std::cout << "no World in file; roots = " << d.roots.size() << "\n";
    }

    // Every non-null object must agree with its slot index.
    for (std::size_t id = 1; id < d.objects_by_id.size(); ++id) {
        if (!d.objects_by_id[id]) continue;
        assert(d.objects_by_id[id]->getObjectID() == static_cast<int>(id));
    }

    // --- scene IR consistency ------------------------------------------------
    // The IR and the object graph should describe the same asset.
    if (!d.scene_ir.nodes.empty() || d.world) {
        std::cout << "IR: nodes="   << d.node_count()
                  << " meshes="     << d.mesh_count()
                  << " materials="  << d.material_count()
                  << " textures="   << d.texture_count()
                  << " images="     << d.image_count()
                  << " cameras="    << d.camera_count()
                  << " animations=" << d.animation_count() << "\n";
    }
    for (const auto &w : d.warnings())
        std::cout << "warning [" << w.code << "]: " << w.message << "\n";

    // Root node indices must be in range.
    for (int r : d.scene_ir.root_node_indices)
        assert(r >= 0 && r < static_cast<int>(d.scene_ir.nodes.size()));

    // Child indices must be in range too.
    for (const auto &n : d.scene_ir.nodes)
        for (int c : n.children)
            assert(c >= 0 && c < static_cast<int>(d.scene_ir.nodes.size()));

    std::cout << "test_decode_walk: OK\n";
    return 0;
}
