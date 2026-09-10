/* Integration: decode assets/90.m3g via Decoder (needs backends linked). */
#include "testfw.h"

#include <m3g.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(void)
{
    TESTFW_INIT();

    namespace fs = std::filesystem;
    const fs::path asset = fs::path("assets") / "90.m3g";

    TESTFW_TEST_BEGIN("assets/90.m3g exists");
    TESTFW_EXPECTED(fs::exists(asset));
    TESTFW_TEST_END();

    if (!fs::exists(asset)) {
        return TESTFW_SUMMARY();
    }

    /* Adapters auto-register when linked (deflate_io_miniz, image_io_stb). */
    TESTFW_TEST_BEGIN("decode_file produces scene IR");
    {
        m3g::decode::Decoder dec;
        m3g::decode::Decoded d = dec.decode_file(asset.string());

        TESTFW_EXPECTED(!d.source_path.empty());
        TESTFW_EXPECTED(d.file.header != nullptr || !d.file.objects_by_id.empty());
        TESTFW_EXPECTED(d.node_count() > 0u);
        TESTFW_EXPECTED(d.mesh_count() > 0u);
        TESTFW_EXPECTED(d.file.world_or_null() != nullptr);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("decode_bytes matches decode_file tallies");
    {
        m3g::decode::Decoder dec;
        auto from_path = dec.decode_file(asset.string());

        std::ifstream in(asset, std::ios::binary);
        TESTFW_EXPECTED(static_cast<bool>(in));
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        TESTFW_EXPECTED(!bytes.empty());

        auto from_bytes = dec.decode_bytes(bytes, asset.string());
        TESTFW_EXPECTED(from_bytes.node_count() == from_path.node_count());
        TESTFW_EXPECTED(from_bytes.mesh_count() == from_path.mesh_count());
        TESTFW_EXPECTED(from_bytes.material_count() == from_path.material_count());
    }
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
