// test_build_scene.cpp
#define M3G_IMPL
#include "m3g.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

static bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

int main() {
    // --- construct -----------------------------------------------------------
    auto world = std::make_shared<m3g::World>();
    auto mesh  = std::make_shared<m3g::Mesh>();
    auto cam   = std::make_shared<m3g::Camera>();
    auto light = std::make_shared<m3g::Light>();

    mesh->setTranslation(1.f, 2.f, 3.f);
    mesh->setScale(2.f, 2.f, 2.f);
    world->addChild(mesh);
    world->addChild(light);
    world->setActiveCamera(cam);

    assert(world->getChildCount() == 2);
    assert(world->getActiveCamera() == cam);
    assert(mesh->getParent() == world.get());
    assert(world->getChild(0) == mesh);

    // --- Transformable composite --------------------------------------------
    // Component transform T(1,2,3)·S(2,2,2) applied to origin -> (1,2,3).
    m3g::Transform xf;
    mesh->getCompositeTransform(&xf);
    float v[4] = {0.f, 0.f, 0.f, 1.f};
    xf.transform(v);
    assert(near(v[0], 1.f) && near(v[1], 2.f) && near(v[2], 3.f));

    // Scale should show up when transforming a unit X vector.
    float ux[4] = {1.f, 0.f, 0.f, 0.f};   // w=0 -> direction, ignores translation
    xf.transform(ux);
    assert(near(ux[0], 2.f) && near(ux[1], 0.f) && near(ux[2], 0.f));

    // --- Camera projection round-trip ---------------------------------------
    cam->setPerspective(60.f, 4.f / 3.f, 0.5f, 500.f);
    float params[4];
    assert(cam->getProjection(params) == m3g::Camera::PERSPECTIVE);
    assert(near(params[0], 60.f) && near(params[2], 0.5f) && near(params[3], 500.f));

    // Invalid params must throw (spec-documented constraint).
    bool threw = false;
    try { cam->setPerspective(200.f, 1.f, 0.1f, 10.f); }
    catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    // --- Light validation ---------------------------------------------------
    threw = false;
    try { light->setSpotAngle(120.f); }        // must be in [0,90]
    catch (const std::invalid_argument &) { threw = true; }
    assert(threw);
    light->setMode(m3g::Light::OMNI);
    assert(light->getMode() == m3g::Light::OMNI);

    // --- duplicate() --------------------------------------------------------
    auto dup = std::dynamic_pointer_cast<m3g::Mesh>(mesh->duplicate());
    assert(dup);
    assert(dup.get() != mesh.get());
    assert(dup->getClassName() == std::string("Mesh"));

    // --- removeChild detaches parent ---------------------------------------
    world->removeChild(mesh);
    assert(world->getChildCount() == 1);
    assert(mesh->getParent() == nullptr);

    std::cout << "test_build_scene: OK\n";
    return 0;
}
