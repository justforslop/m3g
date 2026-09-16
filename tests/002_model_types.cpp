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

    TESTFW_TEST_BEGIN("Object::type_name via Header / World (Java-like names)");
    {
        Header h;
        TESTFW_EXPECTED(h.type_name() == "Header");
        World w;
        TESTFW_EXPECTED(w.type_name() == "World");
        TESTFW_EXPECTED(w.getUserID() == 0);
        w.setUserID(42);
        TESTFW_EXPECTED(w.getUserID() == 42);
        Mesh mesh;
        TESTFW_EXPECTED(mesh.type_name() == "Mesh");
        TESTFW_EXPECTED(mesh.getSubmeshCount() == 0);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("Java-like constants on Camera / Light / Image2D");
    TESTFW_EXPECTED(Camera::PERSPECTIVE == 50);
    TESTFW_EXPECTED(Light::DIRECTIONAL == 129);
    TESTFW_EXPECTED(Image2D::RGBA == 100);
    TESTFW_EXPECTED(KeyframeSequence::LOOP == 193);
    TESTFW_EXPECTED(CompositingMode::REPLACE == 68);
    TESTFW_EXPECTED(ObjectTypes::SPRITE_3D == 18);
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("Java package aliases + Transform / Graphics3D");
    {
        m3g::World w;
        TESTFW_EXPECTED(w.type_name() == "World");
        m3g::Transform t;
        float m[16];
        t.get(m);
        TESTFW_EXPECTED(std::fabs(m[0] - 1.f) < 1e-5f);
        t.postTranslate(1.f, 2.f, 3.f);
        t.get(m);
        TESTFW_EXPECTED(std::fabs(m[12] - 1.f) < 1e-5f);
        auto &g3d = m3g::Graphics3D::getInstance();
        g3d.setViewport(0, 0, 640, 480);
        TESTFW_EXPECTED(g3d.getViewportWidth() == 640);
        m3g::MorphingMesh mm;
        TESTFW_EXPECTED(mm.type_name() == "MorphingMesh");
        m3g::Sprite3D sp;
        TESTFW_EXPECTED(sp.type_name() == "Sprite3D");
        m3g::IndexBuffer *ib = new m3g::TriangleStripArray();
        TESTFW_EXPECTED(ib->type_name() == "TriangleStripArray");
        delete ib;
    }
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
