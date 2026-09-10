/* Unit tests: m3g::model constants and helpers (header-only). */
#include "testfw.h"

#include <m3g.hpp>

#include <cmath>
#include <string>
#include <vector>

int main(void)
{
    TESTFW_INIT();

    using namespace m3g::model;

    TESTFW_TEST_BEGIN("ObjectTypes: known constants");
    TESTFW_EXPECTED(ObjectTypes::HEADER == 0);
    TESTFW_EXPECTED(ObjectTypes::WORLD == 22);
    TESTFW_EXPECTED(ObjectTypes::EXTERNAL_REFERENCE == 0xFF);
    TESTFW_EXPECTED(ObjectTypes::MESH == 14);
    TESTFW_EXPECTED(ObjectTypes::IMAGE_2D == 10);
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("AnimationProperty / KeyframeInterpolation ranges");
    TESTFW_EXPECTED(AnimationProperty::TRANSLATION == 275);
    TESTFW_EXPECTED(AnimationProperty::ORIENTATION == 268);
    TESTFW_EXPECTED(KeyframeInterpolation::LINEAR == 176);
    TESTFW_EXPECTED(KeyframeInterpolation::STEP == 180);
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("type_name_for_object_type");
    TESTFW_EXPECTED(type_name_for_object_type(ObjectTypes::HEADER) == "Header");
    TESTFW_EXPECTED(type_name_for_object_type(ObjectTypes::WORLD) == "World");
    TESTFW_EXPECTED(type_name_for_object_type(ObjectTypes::MESH) == "Mesh");
    TESTFW_EXPECTED(type_name_for_object_type(ObjectTypes::EXTERNAL_REFERENCE) == "ExternalReference");
    TESTFW_EXPECTED(type_name_for_object_type(9999) == "Type9999");
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("RgbColor / RgbaColor to_float_array");
    {
        RgbColor c{255, 128, 0};
        auto f = c.to_float_array();
        TESTFW_EXPECTED(f.size() == 3u);
        TESTFW_EXPECTED(std::fabs(f[0] - 1.f) < 1e-5f);
        TESTFW_EXPECTED(std::fabs(f[1] - (128.f / 255.f)) < 1e-5f);
        TESTFW_EXPECTED(std::fabs(f[2] - 0.f) < 1e-5f);

        RgbaColor a{0, 0, 0, 255};
        auto fa = a.to_float_array();
        TESTFW_EXPECTED(fa.size() == 4u);
        TESTFW_EXPECTED(std::fabs(fa[3] - 1.f) < 1e-5f);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("is_identity_row_major");
    {
        std::vector<float> id = {
            1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        };
        TESTFW_EXPECTED(is_identity_row_major(id));
        id[0] = 1.001f;
        TESTFW_EXPECTED(!is_identity_row_major(id, 1e-5f));
        TESTFW_EXPECTED(is_identity_row_major(id, 0.01f));
        TESTFW_EXPECTED(!is_identity_row_major(std::vector<float>{1, 0, 0}));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("Object::type_name via HeaderObject");
    {
        HeaderObject h;
        h.object_type = ObjectTypes::HEADER;
        TESTFW_EXPECTED(h.type_name() == "Header");
    }
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
