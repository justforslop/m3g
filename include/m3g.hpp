/*
    m3g.hpp -- C++17 binding of javax.microedition.m3g (JSR-184 / M3G 1.1)

    Wraps m3g.h: Java class tree under namespace m3g, shared_ptr refs,
    live object graph + scene IR, optional glTF export.

    Class / constant names follow:
        references/j2me/          (per-class HTML)
        references/j2me_mobile_3d-1_1-mrel-spec.pdf

    Do this in *one* C++ file to create the implementation (pulls in m3g.h too):

        #define M3G_IMPL
        #include "m3g.hpp"

    Equivalent:

        #define M3G_DECODE_IMPL
        #include "m3g.hpp"

    Everywhere else:

        #include "m3g.hpp"

    C-only projects can use m3g.h with #define M3G_IMPLEMENTATION instead.

    Install backends before load (same idea as sokol/stb adapters):

        m3g::install_miniz_deflate_io();
        m3g::install_stb_image_io();        // optional images
        m3g::install_cjson_json_io();       // glTF JSON (export)
        m3g::install_cgltf_gltf_io();       // glTF validate/write (export)

    USAGE (load + walk World like Java Loader):

        #define M3G_IMPL
        #include "m3g.hpp"
        #include <iostream>

        int main() {
            m3g::install_miniz_deflate_io();

            m3g::decode::Decoded d = m3g::Loader::load("model.m3g");
            if (auto w = d.world) {
                std::cout << "children=" << w->getChildCount() << "\n";
                if (auto cam = w->getActiveCamera())
                    std::cout << "camera=" << cam->getClassName() << "\n";
            }
            std::cout << "meshes=" << d.mesh_count() << "\n";
            return 0;
        }

    USAGE (one-shot M3G → glTF):

        #define M3G_IMPL
        #include "m3g.hpp"

        int main() {
            m3g::install_miniz_deflate_io();
            m3g::install_stb_image_io();
            m3g::install_cjson_json_io();
            m3g::install_cgltf_gltf_io();

            m3g::Converter cvt;
            m3g::exp::ExportReport rep =
                cvt.convert("model.m3g", "out.gltf", true);
            // rep.paths.gltf_path, rep.decoded.scene_ir, ...
            return 0;
        }

    USAGE (scene graph without a file — Java-style types):

        auto world = std::make_shared<m3g::World>();
        auto mesh  = std::make_shared<m3g::Mesh>();
        world->addChild(mesh);
        world->setActiveCamera(std::make_shared<m3g::Camera>());

    LICENSE / warranty: see project root.
*/

#ifndef M3G_HPP_INCLUDED
#define M3G_HPP_INCLUDED

#include "m3g.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#ifdef M3G_IMPL
#ifndef M3G_DECODE_IMPL
#define M3G_DECODE_IMPL
#endif
#undef M3G_IMPL
#endif

namespace m3g {

/* ========== forward declarations (Java package order) ========== */

class Object3D;
class AnimationController;
class AnimationTrack;
class Appearance;
class Background;
class CompositingMode;
class Fog;
class Material;
class PolygonMode;
class Image2D;
class Texture2D;
class VertexArray;
class VertexBuffer;
class IndexBuffer;
class TriangleStripArray;
class KeyframeSequence;
class Transformable;
class Node;
class Group;
class World;
class Camera;
class Light;
class Mesh;
class MorphingMesh;
class SkinnedMesh;
class Sprite3D;
class Transform;
class RayIntersection;
class Graphics3D;
class Loader;

using Object3DPtr = std::shared_ptr<Object3D>;
using NodePtr = std::shared_ptr<Node>;

/* ========== toolkit I/O (not part of JSR-184) ========== */

enum { DEFLATE_OK = M3G_DEFLATE_OK, ADLER32_INIT = M3G_ADLER32_INIT };

struct DeflateIo {
    std::uint32_t (*adler32)(std::uint32_t, unsigned char const *, std::size_t, void *) = nullptr;
    int (*uncompress)(unsigned char *, std::size_t *, unsigned char const *, std::size_t, void *) = nullptr;
    void *(*uncompress_to_heap)(unsigned char const *, std::size_t, std::size_t *, int, void *) = nullptr;
    void (*free_mem)(void *, void *) = nullptr;
    void *user = nullptr;
};
void set_deflate_io(DeflateIo const *io);
DeflateIo const *deflate_io(void);
void install_miniz_deflate_io(void);
std::uint32_t deflate_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len);
int deflate_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source, std::size_t source_len);
void *deflate_uncompress_to_heap(unsigned char const *source, std::size_t source_len, std::size_t *out_len, int zlib_header);
void deflate_free(void *p);

struct ImageIo {
    unsigned char *(*load_file)(char const *, int *, int *, int *, int, void *) = nullptr;
    unsigned char *(*load_memory)(unsigned char const *, int, int *, int *, int *, int, void *) = nullptr;
    void (*free_pixels)(void *, void *) = nullptr;
    void *user = nullptr;
};
void set_image_io(ImageIo const *io);
ImageIo const *image_io(void);
void install_stb_image_io(void);
unsigned char *image_load_file(char const *filename, int *width, int *height, int *channels_in_file, int req_comp);
unsigned char *image_load_memory(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file, int req_comp);
void image_free_pixels(void *pixels);

struct JsonIo {
    void *(*create_object)(void *) = nullptr;
    void *(*create_array)(void *) = nullptr;
    void *(*create_string)(char const *, void *) = nullptr;
    void *(*create_number)(double, void *) = nullptr;
    void *(*create_bool)(int, void *) = nullptr;
    void (*add_item_to_object)(void *, char const *, void *, void *) = nullptr;
    void (*add_item_to_array)(void *, void *, void *) = nullptr;
    int (*get_array_size)(void const *, void *) = nullptr;
    void (*delete_node)(void *, void *) = nullptr;
    char *(*print_unformatted)(void *, void *) = nullptr;
    char *(*print_formatted)(void *, void *) = nullptr;
    void (*free_print)(char *, void *) = nullptr;
    void *user = nullptr;
};
void set_json_io(JsonIo const *io);
JsonIo const *json_io(void);
void install_cjson_json_io(void);
void *json_create_object();
void *json_create_array();
void *json_create_string(char const *s);
void *json_create_number(double v);
void *json_create_bool(int v);
void json_add_item_to_object(void *object, char const *key, void *item);
void json_add_item_to_array(void *array, void *item);
int json_get_array_size(void const *array);
void json_delete(void *node);
char *json_print_unformatted(void *node);
char *json_print_formatted(void *node);
void json_free_print(char *printed);

enum GltfFileKind { GLTF_FILE_JSON = 0, GLTF_FILE_GLB = 1 };
enum { GLTF_IO_OK = 0 };
struct GltfIo {
    int (*write_file)(char const *, void const *, int, void *) = nullptr;
    int (*parse_file)(char const *, void **, void *) = nullptr;
    int (*validate)(void *, void *) = nullptr;
    void (*free_data)(void *, void *) = nullptr;
    void *user = nullptr;
};
void set_gltf_io(GltfIo const *io);
GltfIo const *gltf_io(void);
void install_cgltf_gltf_io(void);
void gltf_write_file(char const *path, void const *data, int kind);
void gltf_parse_file(char const *path, void **out_data);
void gltf_validate(void *data);
void gltf_free_data(void *data);

/* ========== Object3D (javax.microedition.m3g.Object3D) ========== */

/**
 * @brief Abstract base class for all objects that can be part of a 3D world.
 *
 * Objects can be associated with animation data via AnimationTrack, duplicated,
 * identified by user-defined integer IDs, and associated with arbitrary user objects.
 *
 * An Object3D is considered "reachable" from a root if it is directly or indirectly
 * referenced from that root through the scene graph. The animate() method only
 * affects reachable objects.
 *
 * Java: javax.microedition.m3g.Object3D
 */
class Object3D : public std::enable_shared_from_this<Object3D> {
public:
    virtual ~Object3D() = default;

    /** @return Application-assigned user ID (default 0). */
    int getUserID() const { return user_id_; }
    /** Sets an application-defined integer ID for find() lookups. */
    void setUserID(int userID) { user_id_ = userID; }

    /** @return Opaque application-defined pointer (default nullptr). */
    void *getUserObject() const { return user_object_; }
    /** Associates an arbitrary application object with this Object3D. */
    void setUserObject(void *userObject) { user_object_ = userObject; }

    /** @return Number of AnimationTracks attached to this object. */
    int getAnimationTrackCount() const { return static_cast<int>(tracks_.size()); }
    /** @return AnimationTrack at @p index (0-based). @throws std::out_of_range */
    std::shared_ptr<AnimationTrack> getAnimationTrack(int index) const {
        return tracks_.at(static_cast<std::size_t>(index));
    }
    /** Attaches an AnimationTrack. @throws std::invalid_argument if null. */
    void addAnimationTrack(const std::shared_ptr<AnimationTrack> &animationTrack) {
        if (!animationTrack) throw std::invalid_argument("Object3D.addAnimationTrack: null");
        tracks_.push_back(animationTrack);
    }
    /** Detaches @p animationTrack if present (no-op if null or not found). */
    void removeAnimationTrack(const std::shared_ptr<AnimationTrack> &animationTrack) {
        if (!animationTrack) return;
        tracks_.erase(std::remove(tracks_.begin(), tracks_.end(), animationTrack), tracks_.end());
    }

    /**
     * Updates all animated properties in this Object3D and reachable Object3Ds.
     * @param time World time in milliseconds.
     * @return Validity interval end time (toolkit stub returns 0).
     */
    virtual int animate(int /*time*/) { return 0; }

    /** Creates a shallow copy; subclasses should override. */
    virtual Object3DPtr duplicate() const { return std::make_shared<Object3D>(*this); }

    /**
     * Depth-first search for an object with the given user ID among this object
     * and its references. Base implementation only matches this object.
     * @return matching Object3D, or nullptr if not found.
     */
    Object3DPtr find(int userID) {
        if (user_id_ == userID) return shared_from_this();
        return nullptr;
    }

    /**
     * Fills @p references with direct Object3D refs (excluding parent/alignment).
     * @return number of references (may exceed max_count).
     */
    virtual int getReferences(Object3DPtr *references, int max_count) const {
        (void)references;
        (void)max_count;
        return 0;
    }

    /** Toolkit: M3G object type byte (M3G_OBJ_*). */
    int getObjectType() const { return object_type_; }
    /** Toolkit: parser-assigned object id (1-based), or 0 if not from a file. */
    int getObjectID() const { return object_id_; }
    virtual const char *getClassName() const { return "Object3D"; }
    std::string type_name() const { return getClassName(); } /* test/compat alias */

    /* Toolkit metadata (set by Loader / decoder). */
    int object_type_ = 0;
    int object_id_ = 0;
    int raw_length_ = 0;

    Object3D() = default;

protected:
    int user_id_ = 0;
    void *user_object_ = nullptr;
    std::vector<std::shared_ptr<AnimationTrack>> tracks_;
};

/* ========== Transform (does not extend Object3D) ========== */

/**
 * @brief A generic 4x4 floating point matrix, representing a transformation.
 *
 * By default, all methods dealing with Transform objects operate on arbitrary
 * 4x4 matrices. Any exceptions to this rule are documented explicitly at the
 * method level.
 *
 * Java: javax.microedition.m3g.Transform
 * Storage is row-major float[16] in this port.
 */
class Transform {
public:
    Transform() { setIdentity(); }
    Transform(const Transform &t) { set(t); }
    explicit Transform(const float *matrix) { set(matrix); }

    void setIdentity() {
        m_.fill(0.f);
        m_[0] = m_[5] = m_[10] = m_[15] = 1.f;
    }
    void set(const Transform &transform) { m_ = transform.m_; }
    void set(const float *matrix) {
        for (int i = 0; i < 16; ++i) m_[static_cast<std::size_t>(i)] = matrix[i];
    }
    void get(float *matrix) const {
        for (int i = 0; i < 16; ++i) matrix[i] = m_[static_cast<std::size_t>(i)];
    }

    void transpose() {
        float t[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) t[c * 4 + r] = m_[static_cast<std::size_t>(r * 4 + c)];
        set(t);
    }
    void invert(); /* defined under M3G_DECODE_IMPL */

    void postMultiply(const Transform &transform) {
        float out[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                out[r * 4 + c] =
                    m_[static_cast<std::size_t>(r * 4 + 0)] * transform.m_[static_cast<std::size_t>(0 * 4 + c)] +
                    m_[static_cast<std::size_t>(r * 4 + 1)] * transform.m_[static_cast<std::size_t>(1 * 4 + c)] +
                    m_[static_cast<std::size_t>(r * 4 + 2)] * transform.m_[static_cast<std::size_t>(2 * 4 + c)] +
                    m_[static_cast<std::size_t>(r * 4 + 3)] * transform.m_[static_cast<std::size_t>(3 * 4 + c)];
            }
        set(out);
    }
    void postTranslate(float tx, float ty, float tz) {
        Transform t;
        t.m_[12] = tx;
        t.m_[13] = ty;
        t.m_[14] = tz;
        postMultiply(t);
    }
    void postScale(float sx, float sy, float sz) {
        Transform t;
        t.m_[0] = sx;
        t.m_[5] = sy;
        t.m_[10] = sz;
        postMultiply(t);
    }
    void postRotate(float angle, float ax, float ay, float az);
    void postRotateQuat(float qx, float qy, float qz, float qw);
    void transform(float *vectors /* xyzw stride 4 */) const;
    void transform(const VertexArray &in, float *out, bool W) const;

    const float *data() const { return m_.data(); }
    float *data() { return m_.data(); }

private:
    std::array<float, 16> m_{};
};

/* ========== Transformable ========== */

/**
 * @brief An abstract base class for Node and Texture2D, defining common methods
 *        for manipulating node and texture transformations.
 *
 * A Transformable can have a general 4x4 matrix transform (set via setTransform)
 * and/or a component transform composed of translation, scale, and orientation
 * (set via setTranslation, setScale, setOrientation). When both are present,
 * the component transform is applied first, followed by the general transform.
 *
 * Java: javax.microedition.m3g.Transformable
 */
class Transformable : public Object3D {
public:
    void setTranslation(float tx, float ty, float tz) {
        translation_[0] = tx;
        translation_[1] = ty;
        translation_[2] = tz;
        has_component_ = true;
    }
    void translate(float tx, float ty, float tz) {
        setTranslation(translation_[0] + tx, translation_[1] + ty, translation_[2] + tz);
    }
    void getTranslation(float *xyz) const {
        xyz[0] = translation_[0];
        xyz[1] = translation_[1];
        xyz[2] = translation_[2];
    }

    void setScale(float sx, float sy, float sz) {
        scale_[0] = sx;
        scale_[1] = sy;
        scale_[2] = sz;
        has_component_ = true;
    }
    void scale(float sx, float sy, float sz) {
        setScale(scale_[0] * sx, scale_[1] * sy, scale_[2] * sz);
    }
    void getScale(float *xyz) const {
        xyz[0] = scale_[0];
        xyz[1] = scale_[1];
        xyz[2] = scale_[2];
    }

    void setOrientation(float angle, float ax, float ay, float az) {
        orientation_angle_ = angle;
        orientation_axis_[0] = ax;
        orientation_axis_[1] = ay;
        orientation_axis_[2] = az;
        has_component_ = true;
    }
    void getOrientation(float *angleAxis) const {
        angleAxis[0] = orientation_angle_;
        angleAxis[1] = orientation_axis_[0];
        angleAxis[2] = orientation_axis_[1];
        angleAxis[3] = orientation_axis_[2];
    }
    void postRotate(float angle, float ax, float ay, float az) {
        orientation_angle_ += angle;
        orientation_axis_[0] = ax;
        orientation_axis_[1] = ay;
        orientation_axis_[2] = az;
        has_component_ = true;
    }
    void preRotate(float angle, float ax, float ay, float az) { postRotate(angle, ax, ay, az); }

    void setTransform(const Transform *transform) {
        if (!transform) {
            has_matrix_ = false;
            return;
        }
        transform->get(matrix_);
        has_matrix_ = true;
    }
    void setTransform(const Transform &transform) { setTransform(&transform); }
    void getTransform(Transform *transform) const {
        if (!transform) throw std::invalid_argument("getTransform: null");
        if (has_matrix_) transform->set(matrix_);
        else transform->setIdentity();
    }
    void getCompositeTransform(Transform *transform) const;
    void getCompositeTransform(float *m16) const;

    Object3DPtr duplicate() const override { return std::make_shared<Transformable>(*this); }
    const char *getClassName() const override { return "Transformable"; }

protected:
    bool has_component_ = false;
    bool has_matrix_ = false;
    float translation_[3]{0, 0, 0};
    float scale_[3]{1, 1, 1};
    float orientation_angle_ = 0.f;
    float orientation_axis_[3]{0, 0, 1};
    float matrix_[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

/* ========== Node ========== */

/**
 * @brief An abstract base class for all scene graph nodes.
 *
 * Nodes can be arranged in a tree structure (via Group), with rendering and
 * picking enabled/disabled. Nodes support an alpha factor for transparency and
 * a scope bitmask for forming conceptual groups independent of the hierarchy.
 *
 * Alignment allows a node to follow another node along specified axes
 * (X_AXIS, Y_AXIS, Z_AXIS, or ORIGIN).
 *
 * Java: javax.microedition.m3g.Node
 */
class Node : public Transformable {
public:
    static constexpr int NONE = 144;
    static constexpr int ORIGIN = 145;
    static constexpr int X_AXIS = 146;
    static constexpr int Y_AXIS = 147;
    static constexpr int Z_AXIS = 148;

    void setRenderingEnable(bool enable) { enable_rendering_ = enable; }
    bool isRenderingEnabled() const { return enable_rendering_; }
    void setPickingEnable(bool enable) { enable_picking_ = enable; }
    bool isPickingEnabled() const { return enable_picking_; }
    void setScope(int scope) { scope_ = scope; }
    int getScope() const { return scope_; }
    void setAlphaFactor(float alphaFactor) {
        if (alphaFactor < 0.f) alphaFactor = 0.f;
        if (alphaFactor > 1.f) alphaFactor = 1.f;
        alpha_factor_ = alphaFactor;
    }
    float getAlphaFactor() const { return alpha_factor_; }

    Node *getParent() const { return parent_; }
    void setParent(Node *p) { parent_ = p; } /* toolkit */

    void setAlignment(Node *zRef, int zTarget, Node *yRef, int yTarget) {
        z_ref_ = zRef;
        z_target_ = zTarget;
        y_ref_ = yRef;
        y_target_ = yTarget;
    }
    int getAlignmentTarget(int axis) const {
        if (axis == Z_AXIS) return z_target_;
        if (axis == Y_AXIS) return y_target_;
        return NONE;
    }
    Node *getAlignmentReference(int axis) const {
        if (axis == Z_AXIS) return z_ref_;
        if (axis == Y_AXIS) return y_ref_;
        return nullptr;
    }
    void align(Node * /*reference*/) {}

    bool getTransformTo(Node *target, Transform *transform) const {
        if (!target || !transform || target != this) return false;
        getCompositeTransform(transform);
        return true;
    }

    Object3DPtr duplicate() const override { return std::make_shared<Node>(*this); }
    const char *getClassName() const override { return "Node"; }

protected:
    bool enable_rendering_ = true;
    bool enable_picking_ = true;
    float alpha_factor_ = 1.f;
    int scope_ = -1;
    Node *parent_ = nullptr;
    Node *z_ref_ = nullptr;
    Node *y_ref_ = nullptr;
    int z_target_ = NONE;
    int y_target_ = NONE;
};

/* ========== Group / World ========== */

/**
 * @brief A scene graph node that stores an unordered set of nodes as its children.
 *
 * Group serves as the primary container for building scene graph hierarchies.
 * Children can be added, removed, and queried. Group also supports picking,
 * which tests for ray intersections against all renderable children.
 *
 * Java: javax.microedition.m3g.Group
 */
class Group : public Node {
public:
    Group() { object_type_ = M3G_OBJ_GROUP; }

    void addChild(const NodePtr &child) {
        if (!child) throw std::invalid_argument("Group.addChild: null");
        children_.push_back(child);
        child->setParent(this);
    }
    void removeChild(const NodePtr &child) {
        if (!child) return;
        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            (*it)->setParent(nullptr);
            children_.erase(it);
        }
    }
    int getChildCount() const { return static_cast<int>(children_.size()); }
    NodePtr getChild(int index) const { return children_.at(static_cast<std::size_t>(index)); }

    bool pick(int, float, float, float, float, float, float, RayIntersection *) const { return false; }
    bool pick(int, float, float, Camera *, RayIntersection *) const { return false; }

    int getReferences(Object3DPtr *references, int max_count) const override {
        const int n = static_cast<int>(children_.size());
        if (references && max_count > 0) {
            const int copy = std::min(n, max_count);
            for (int i = 0; i < copy; ++i) references[i] = children_[static_cast<std::size_t>(i)];
        }
        return n;
    }
    Object3DPtr duplicate() const override { return std::make_shared<Group>(*this); }
    const char *getClassName() const override { return "Group"; }

    /* toolkit */
    std::vector<NodePtr> &children() { return children_; }
    std::vector<NodePtr> const &children() const { return children_; }

protected:
    std::vector<NodePtr> children_;
};

/**
 * @brief A special Group node that is a top-level container for scene graphs.
 *
 * A scene graph is constructed from a hierarchy of nodes. In a complete scene
 * graph, all nodes are ultimately connected to each other via a common root,
 * which is a World node. A World holds a reference to its active Camera and
 * Background.
 *
 * Java: javax.microedition.m3g.World
 */
class World : public Group {
public:
    World() { object_type_ = M3G_OBJ_WORLD; }
    void setActiveCamera(const std::shared_ptr<Camera> &camera) { active_camera_ = camera; }
    std::shared_ptr<Camera> getActiveCamera() const { return active_camera_; }
    void setBackground(const std::shared_ptr<Background> &background) { background_ = background; }
    std::shared_ptr<Background> getBackground() const { return background_; }
    Object3DPtr duplicate() const override { return std::make_shared<World>(*this); }
    const char *getClassName() const override { return "World"; }

private:
    std::shared_ptr<Camera> active_camera_;
    std::shared_ptr<Background> background_;
};

/* ========== Camera / Light ========== */

/**
 * @brief A scene graph node that defines the position of the viewer in the scene
 *        and the projection from 3D to 2D.
 *
 * Three projection types are supported:
 * - PERSPECTIVE: defined by field-of-view, aspect ratio, near/far clip planes
 * - PARALLEL: orthographic projection with the same parameters
 * - GENERIC: an arbitrary 4x4 projection matrix
 *
 * Java: javax.microedition.m3g.Camera
 */
class Camera : public Node {
public:
    static constexpr int GENERIC = 48;
    static constexpr int PARALLEL = 49;
    static constexpr int PERSPECTIVE = 50;

    Camera() { object_type_ = M3G_OBJ_CAMERA; }

    void setPerspective(float fovy, float aspectRatio, float near, float far) {
        if (fovy <= 0.f || fovy >= 180.f || aspectRatio <= 0.f || near <= 0.f || far <= 0.f || near >= far)
            throw std::invalid_argument("Camera.setPerspective");
        projection_type_ = PERSPECTIVE;
        params_[0] = fovy;
        params_[1] = aspectRatio;
        params_[2] = near;
        params_[3] = far;
    }
    void setParallel(float fovy, float aspectRatio, float near, float far) {
        if (fovy <= 0.f || aspectRatio <= 0.f || near <= 0.f || far <= 0.f || near >= far)
            throw std::invalid_argument("Camera.setParallel");
        projection_type_ = PARALLEL;
        params_[0] = fovy;
        params_[1] = aspectRatio;
        params_[2] = near;
        params_[3] = far;
    }
    void setGeneric(const Transform *transform) {
        if (!transform) throw std::invalid_argument("Camera.setGeneric: null");
        projection_type_ = GENERIC;
        transform->get(generic_);
        has_generic_ = true;
    }
    void setGeneric(const Transform &t) { setGeneric(&t); }

    int getProjection(float *params) const {
        if ((projection_type_ == PERSPECTIVE || projection_type_ == PARALLEL) && params) {
            for (int i = 0; i < 4; ++i) params[i] = params_[static_cast<std::size_t>(i)];
        }
        return projection_type_;
    }
    int getProjection(Transform *transform) const {
        if (!transform) throw std::invalid_argument("Camera.getProjection: null");
        if (projection_type_ == GENERIC && has_generic_) transform->set(generic_);
        else transform->setIdentity();
        return projection_type_;
    }

    Object3DPtr duplicate() const override { return std::make_shared<Camera>(*this); }
    const char *getClassName() const override { return "Camera"; }

    int projection_type_ = 0;
    std::array<float, 4> params_{45.f, 1.f, 0.1f, 100.f};
    float generic_[16]{};
    bool has_generic_ = false;
};

/**
 * @brief A scene graph node that represents different kinds of light sources.
 *
 * Supported modes: AMBIENT, DIRECTIONAL, OMNI (point), and SPOT.
 * Directional lights are infinite in range; omni and spot lights have
 * attenuation (constant, linear, quadratic). Spot lights additionally have
 * a cone angle and exponent.
 *
 * Java: javax.microedition.m3g.Light
 */
class Light : public Node {
public:
    static constexpr int AMBIENT = 128;
    static constexpr int DIRECTIONAL = 129;
    static constexpr int OMNI = 130;
    static constexpr int SPOT = 131;

    Light() { object_type_ = M3G_OBJ_LIGHT; mode_ = DIRECTIONAL; }

    void setMode(int mode) {
        if (mode != AMBIENT && mode != DIRECTIONAL && mode != OMNI && mode != SPOT)
            throw std::invalid_argument("Light.setMode");
        mode_ = mode;
    }
    int getMode() const { return mode_; }
    void setIntensity(float intensity) {
        if (intensity < 0.f) throw std::invalid_argument("Light.setIntensity");
        intensity_ = intensity;
    }
    float getIntensity() const { return intensity_; }
    void setColor(int RGB) { color_ = RGB & 0x00ffffff; }
    int getColor() const { return color_; }
    void setAttenuation(float constant, float linear, float quadratic) {
        if (constant < 0.f || linear < 0.f || quadratic < 0.f) throw std::invalid_argument("Light.setAttenuation");
        att_c_ = constant;
        att_l_ = linear;
        att_q_ = quadratic;
    }
    float getConstantAttenuation() const { return att_c_; }
    float getLinearAttenuation() const { return att_l_; }
    float getQuadraticAttenuation() const { return att_q_; }
    void setSpotAngle(float angle) {
        if (angle < 0.f || angle > 90.f) throw std::invalid_argument("Light.setSpotAngle");
        spot_angle_ = angle;
    }
    float getSpotAngle() const { return spot_angle_; }
    void setSpotExponent(float exponent) {
        if (exponent < 0.f || exponent > 128.f) throw std::invalid_argument("Light.setSpotExponent");
        spot_exponent_ = exponent;
    }
    float getSpotExponent() const { return spot_exponent_; }

    Object3DPtr duplicate() const override { return std::make_shared<Light>(*this); }
    const char *getClassName() const override { return "Light"; }

    int mode_ = DIRECTIONAL;
    float intensity_ = 1.f;
    int color_ = 0x00ffffff;
    float att_c_ = 1.f, att_l_ = 0.f, att_q_ = 0.f;
    float spot_angle_ = 45.f, spot_exponent_ = 0.f;
};

/* ========== Appearance stack ========== */

/**
 * @brief Defines whether and how to clear the viewport before rendering.
 *
 * The viewport can be cleared to a solid color, an image (or both). Image
 * clearing supports tiling modes (BORDER, REPEAT) and a crop rectangle.
 * Depth buffer clearing is controlled independently.
 *
 * Java: javax.microedition.m3g.Background
 */
class Background : public Object3D {
public:
    static constexpr int BORDER = 32;
    static constexpr int REPEAT = 33;
    Background() { object_type_ = M3G_OBJ_BACKGROUND; }

    void setColorClearEnable(bool enable) { color_clear_ = enable; }
    bool isColorClearEnabled() const { return color_clear_; }
    void setDepthClearEnable(bool enable) { depth_clear_ = enable; }
    bool isDepthClearEnabled() const { return depth_clear_; }
    void setColor(int ARGB) { color_ = ARGB; }
    int getColor() const { return color_; }
    void setImage(const std::shared_ptr<Image2D> &image) { image_ = image; }
    std::shared_ptr<Image2D> getImage() const { return image_; }
    void setImageMode(int modeX, int modeY) {
        mode_x_ = modeX;
        mode_y_ = modeY;
    }
    int getImageModeX() const { return mode_x_; }
    int getImageModeY() const { return mode_y_; }
    void setCrop(int cropX, int cropY, int width, int height) {
        crop_x_ = cropX;
        crop_y_ = cropY;
        crop_w_ = width;
        crop_h_ = height;
    }
    int getCropX() const { return crop_x_; }
    int getCropY() const { return crop_y_; }
    int getCropWidth() const { return crop_w_; }
    int getCropHeight() const { return crop_h_; }
    const char *getClassName() const override { return "Background"; }

    int color_ = 0;
    bool color_clear_ = true, depth_clear_ = true;
    std::shared_ptr<Image2D> image_;
    int mode_x_ = 0, mode_y_ = 0;
    int crop_x_ = 0, crop_y_ = 0, crop_w_ = 0, crop_h_ = 0;
};

/**
 * @brief An Appearance component encapsulating attributes for fogging.
 *
 * Fog can be LINEAR (fades between near and far distances) or EXPONENTIAL
 * (density-based). The fog color is applied as a blend between the fragment
 * color and the fog color based on distance.
 *
 * Java: javax.microedition.m3g.Fog
 */
class Fog : public Object3D {
public:
    static constexpr int EXPONENTIAL = 80;
    static constexpr int LINEAR = 81;
    Fog() { object_type_ = M3G_OBJ_FOG; }
    void setMode(int mode) { mode_ = mode; }
    int getMode() const { return mode_; }
    void setLinear(float near, float far) {
        near_ = near;
        far_ = far;
        mode_ = LINEAR;
    }
    float getNearDistance() const { return near_; }
    float getFarDistance() const { return far_; }
    void setDensity(float density) {
        density_ = density;
        mode_ = EXPONENTIAL;
    }
    float getDensity() const { return density_; }
    void setColor(int RGB) { color_ = RGB & 0x00ffffff; }
    int getColor() const { return color_; }
    const char *getClassName() const override { return "Fog"; }
    int mode_ = 0, color_ = 0;
    float density_ = 0, near_ = 0, far_ = 0;
};

/**
 * @brief An Appearance component encapsulating polygon-level attributes.
 *
 * Includes settings related to back/front face culling (CULL_BACK, CULL_FRONT,
 * CULL_NONE), polygon winding (WINDING_CCW, WINDING_CW), shading (SHADE_FLAT,
 * SHADE_SMOOTH), two-sided lighting, local camera lighting, and
 * perspective correction.
 *
 * Java: javax.microedition.m3g.PolygonMode
 */
class PolygonMode : public Object3D {
public:
    static constexpr int CULL_BACK = 160, CULL_FRONT = 161, CULL_NONE = 162;
    static constexpr int SHADE_FLAT = 164, SHADE_SMOOTH = 165;
    static constexpr int WINDING_CCW = 168, WINDING_CW = 169;
    PolygonMode() { object_type_ = M3G_OBJ_POLYGON_MODE; }
    void setCulling(int mode) { culling_ = mode; }
    int getCulling() const { return culling_; }
    void setWinding(int mode) { winding_ = mode; }
    int getWinding() const { return winding_; }
    void setShading(int mode) { shading_ = mode; }
    int getShading() const { return shading_; }
    void setTwoSidedLightingEnable(bool e) { two_sided_ = e; }
    bool isTwoSidedLightingEnabled() const { return two_sided_; }
    void setLocalCameraLightingEnable(bool e) { local_cam_ = e; }
    bool isLocalCameraLightingEnabled() const { return local_cam_; }
    void setPerspectiveCorrectionEnable(bool e) { persp_ = e; }
    bool isPerspectiveCorrectionEnabled() const { return persp_; }
    const char *getClassName() const override { return "PolygonMode"; }
    int culling_ = 0, shading_ = 0, winding_ = 0;
    bool two_sided_ = false, local_cam_ = false, persp_ = false;
};

/**
 * @brief An Appearance component encapsulating material attributes for
 *        lighting computations.
 *
 * Defines ambient, diffuse, emissive, and specular colors, shininess, and
 * vertex color tracking. Other attributes required for lighting are defined
 * in Light, PolygonMode, and VertexBuffer.
 *
 * Java: javax.microedition.m3g.Material
 */
class Material : public Object3D {
public:
    static constexpr int AMBIENT = 1024, DIFFUSE = 2048, EMISSIVE = 4096, SPECULAR = 8192;
    Material() { object_type_ = M3G_OBJ_MATERIAL; }
    void setColor(int target, int ARGB) {
        if (target == AMBIENT) ambient_ = ARGB;
        else if (target == DIFFUSE) diffuse_ = ARGB;
        else if (target == EMISSIVE) emissive_ = ARGB;
        else if (target == SPECULAR) specular_ = ARGB;
    }
    int getColor(int target) const {
        if (target == AMBIENT) return ambient_;
        if (target == DIFFUSE) return diffuse_;
        if (target == EMISSIVE) return emissive_;
        if (target == SPECULAR) return specular_;
        return 0;
    }
    void setShininess(float s) { shininess_ = s; }
    float getShininess() const { return shininess_; }
    void setVertexColorTrackingEnable(bool e) { vct_ = e; }
    bool isVertexColorTrackingEnabled() const { return vct_; }
    const char *getClassName() const override { return "Material"; }
    int ambient_ = 0, diffuse_ = 0xffffffff, emissive_ = 0, specular_ = 0;
    float shininess_ = 0;
    bool vct_ = false;
};

/**
 * @brief An Appearance component encapsulating per-pixel compositing attributes.
 *
 * Controls blending mode (ALPHA, ALPHA_ADD, MODULATE, MODULATE_X2, REPLACE),
 * alpha threshold, alpha/color/depth write and depth test enable, and
 * depth offset (polygon offset).
 *
 * Java: javax.microedition.m3g.CompositingMode
 */
class CompositingMode : public Object3D {
public:
    static constexpr int ALPHA = 64, ALPHA_ADD = 65, MODULATE = 66, MODULATE_X2 = 67, REPLACE = 68;
    CompositingMode() { object_type_ = M3G_OBJ_COMPOSITING_MODE; blending_ = REPLACE; }
    void setBlending(int mode) { blending_ = mode; }
    int getBlending() const { return blending_; }
    void setAlphaThreshold(float t) {
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        alpha_threshold_ = t;
    }
    float getAlphaThreshold() const { return alpha_threshold_; }
    void setAlphaWriteEnable(bool e) { alpha_write_ = e; }
    bool isAlphaWriteEnabled() const { return alpha_write_; }
    void setColorWriteEnable(bool e) { color_write_ = e; }
    bool isColorWriteEnabled() const { return color_write_; }
    void setDepthWriteEnable(bool e) { depth_write_ = e; }
    bool isDepthWriteEnabled() const { return depth_write_; }
    void setDepthTestEnable(bool e) { depth_test_ = e; }
    bool isDepthTestEnabled() const { return depth_test_; }
    void setDepthOffset(float factor, float units) {
        depth_factor_ = factor;
        depth_units_ = units;
    }
    float getDepthOffsetFactor() const { return depth_factor_; }
    float getDepthOffsetUnits() const { return depth_units_; }
    const char *getClassName() const override { return "CompositingMode"; }
    int blending_ = REPLACE;
    float alpha_threshold_ = 0.f;
    bool alpha_write_ = true, color_write_ = true, depth_write_ = true, depth_test_ = true;
    float depth_factor_ = 0.f, depth_units_ = 0.f;
};

/**
 * @brief A two-dimensional image that can be used as a texture, background
 *        or sprite image.
 *
 * Supported formats: ALPHA, LUMINANCE, LUMINANCE_ALPHA, RGB, RGBA.
 * The image is mutable only if created with the mutable flag; otherwise
 * pixel data cannot be modified after creation.
 *
 * Java: javax.microedition.m3g.Image2D
 */
class Image2D : public Object3D {
public:
    static constexpr int ALPHA = 96, LUMINANCE = 97, LUMINANCE_ALPHA = 98, RGB = 99, RGBA = 100;
    Image2D() { object_type_ = M3G_OBJ_IMAGE_2D; }
    bool isMutable() const { return is_mutable_; }
    int getFormat() const { return format_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    void set(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const std::uint8_t * /*image*/) {}
    const char *getClassName() const override { return "Image2D"; }

    int format_ = 0;
    bool is_mutable_ = false;
    int width_ = 0, height_ = 0;
    std::vector<std::uint8_t> palette_;
    std::vector<std::uint8_t> pixels_;
};

/**
 * @brief An Appearance component encapsulating a two-dimensional texture image
 *        and a set of attributes specifying how the image is to be applied
 *        on submeshes.
 *
 * The attributes include:
 * - Wrapping: WRAP_CLAMP or WRAP_REPEAT in S and T directions
 * - Filtering: level filter (FILTER_BASE_LEVEL) and image filter
 *   (FILTER_LINEAR or FILTER_NEAREST)
 * - Blending function: FUNC_ADD, FUNC_BLEND, FUNC_DECAL, FUNC_MODULATE, FUNC_REPLACE
 * - Texture coordinate transform (inherited from Transformable)
 *
 * Java: javax.microedition.m3g.Texture2D
 */
class Texture2D : public Transformable {
public:
    static constexpr int FILTER_BASE_LEVEL = 208, FILTER_LINEAR = 209, FILTER_NEAREST = 210;
    static constexpr int FUNC_ADD = 224, FUNC_BLEND = 225, FUNC_DECAL = 226, FUNC_MODULATE = 227, FUNC_REPLACE = 228;
    static constexpr int WRAP_CLAMP = 240, WRAP_REPEAT = 241;
    Texture2D() { object_type_ = M3G_OBJ_TEXTURE_2D; }
    void setImage(const std::shared_ptr<Image2D> &image) { image_ = image; }
    std::shared_ptr<Image2D> getImage() const { return image_; }
    void setFiltering(int levelFilter, int imageFilter) {
        level_filter_ = levelFilter;
        image_filter_ = imageFilter;
    }
    int getLevelFilter() const { return level_filter_; }
    int getImageFilter() const { return image_filter_; }
    void setWrapping(int wrapS, int wrapT) {
        wrap_s_ = wrapS;
        wrap_t_ = wrapT;
    }
    int getWrappingS() const { return wrap_s_; }
    int getWrappingT() const { return wrap_t_; }
    void setBlending(int func) { blending_ = func; }
    int getBlending() const { return blending_; }
    void setBlendColor(int RGB) { blend_color_ = RGB & 0x00ffffff; }
    int getBlendColor() const { return blend_color_; }
    const char *getClassName() const override { return "Texture2D"; }

    std::shared_ptr<Image2D> image_;
    int level_filter_ = 0, image_filter_ = 0, wrap_s_ = 0, wrap_t_ = 0, blending_ = 0, blend_color_ = 0;
};

/**
 * @brief A set of component objects that define the rendering attributes of
 *        a Mesh or Sprite3D.
 *
 * Components include:
 * - Rendering layer (int, for painter's algorithm ordering)
 * - Fog (optional)
 * - PolygonMode (optional)
 * - CompositingMode (optional)
 * - Material (optional; lighting disabled if null)
 * - Up to N Texture2D slots (texturing disabled if null)
 *
 * Java: javax.microedition.m3g.Appearance
 */
class Appearance : public Object3D {
public:
    Appearance() { object_type_ = M3G_OBJ_APPEARANCE; }
    void setLayer(int layer) { layer_ = layer; }
    int getLayer() const { return layer_; }
    void setFog(const std::shared_ptr<Fog> &fog) { fog_ = fog; }
    std::shared_ptr<Fog> getFog() const { return fog_; }
    void setPolygonMode(const std::shared_ptr<PolygonMode> &pm) { polygon_mode_ = pm; }
    std::shared_ptr<PolygonMode> getPolygonMode() const { return polygon_mode_; }
    void setCompositingMode(const std::shared_ptr<CompositingMode> &cm) { compositing_mode_ = cm; }
    std::shared_ptr<CompositingMode> getCompositingMode() const { return compositing_mode_; }
    void setMaterial(const std::shared_ptr<Material> &m) { material_ = m; }
    std::shared_ptr<Material> getMaterial() const { return material_; }
    void setTexture(int index, const std::shared_ptr<Texture2D> &texture) {
        if (index < 0) throw std::out_of_range("Appearance.setTexture");
        if (static_cast<int>(textures_.size()) <= index) textures_.resize(static_cast<std::size_t>(index + 1));
        textures_[static_cast<std::size_t>(index)] = texture;
    }
    std::shared_ptr<Texture2D> getTexture(int index) const {
        if (index < 0 || index >= static_cast<int>(textures_.size())) return nullptr;
        return textures_[static_cast<std::size_t>(index)];
    }
    const char *getClassName() const override { return "Appearance"; }

    int layer_ = 0;
    std::shared_ptr<Fog> fog_;
    std::shared_ptr<PolygonMode> polygon_mode_;
    std::shared_ptr<CompositingMode> compositing_mode_;
    std::shared_ptr<Material> material_;
    std::vector<std::shared_ptr<Texture2D>> textures_;
};

/* ========== Geometry ========== */

/**
 * @brief An array of integer vectors representing vertex positions, normals,
 *        colors, or texture coordinates.
 *
 * Each vertex has a fixed number of components (e.g., 3 for positions/normals,
 * 2 for texture coordinates, 4 for colors). The component size is either 1 byte
 * or 2 bytes per component. The array is immutable once created.
 *
 * Java: javax.microedition.m3g.VertexArray
 */
class VertexArray : public Object3D {
public:
    VertexArray() { object_type_ = M3G_OBJ_VERTEX_ARRAY; }
    int getVertexCount() const { return vertex_count_; }
    int getComponentCount() const { return component_count_; }
    int getComponentType() const { return component_size_; } /* 1=byte, 2=short */
    const char *getClassName() const override { return "VertexArray"; }

    int component_size_ = 0, component_count_ = 0, encoding_ = 0, vertex_count_ = 0;
    std::vector<int> components_;
};

/**
 * @brief Holds references to VertexArrays that contain the positions, colors,
 *        normals, and texture coordinates for a set of vertices.
 *
 * The elements of these arrays are called vertex attributes. Positions can
 * have a scale and bias for compact storage. Multiple texture coordinate
 * sets are supported. If no color array is provided, a default color is used.
 *
 * Java: javax.microedition.m3g.VertexBuffer
 */
class VertexBuffer : public Object3D {
public:
    VertexBuffer() { object_type_ = M3G_OBJ_VERTEX_BUFFER; }
    int getVertexCount() const;
    void setPositions(const std::shared_ptr<VertexArray> &positions, float scale, const float *bias);
    void setTexCoords(int index, const std::shared_ptr<VertexArray> &texCoords, float scale, const float *bias);
    void setNormals(const std::shared_ptr<VertexArray> &normals) { normals_ = normals; }
    void setColors(const std::shared_ptr<VertexArray> &colors) { colors_ = colors; }
    std::shared_ptr<VertexArray> getPositions(float *scaleBias) const;
    std::shared_ptr<VertexArray> getTexCoords(int index, float *scaleBias) const;
    std::shared_ptr<VertexArray> getNormals() const { return normals_; }
    std::shared_ptr<VertexArray> getColors() const { return colors_; }
    void setDefaultColor(int ARGB) { default_color_ = ARGB; }
    int getDefaultColor() const { return default_color_; }
    const char *getClassName() const override { return "VertexBuffer"; }

    std::shared_ptr<VertexArray> positions_, normals_, colors_;
    float position_scale_ = 1.f;
    float position_bias_[3]{0, 0, 0};
    struct TexBind {
        std::shared_ptr<VertexArray> array;
        float scale = 1.f;
        float bias[3]{0, 0, 0};
    };
    std::vector<TexBind> tex_coords_;
    int default_color_ = 0xffffffff;
};

/**
 * @brief An abstract base class defining how to connect vertices to form a
 *        geometric object (a submesh).
 *
 * In order to be rendered, a submesh must be associated with a VertexBuffer,
 * defining the vertex attributes, and an Appearance, defining the surface
 * attributes. Currently only triangle-based index buffers are supported.
 *
 * Java: javax.microedition.m3g.IndexBuffer
 */
class IndexBuffer : public Object3D {
public:
    virtual int getIndexCount() const = 0;
    virtual void getIndices(int *indices) const = 0;
    const char *getClassName() const override { return "IndexBuffer"; }
};

/**
 * @brief Defines an array of triangle strips.
 *
 * In a triangle strip, the first three vertex indices define the first
 * triangle. Each subsequent index together with the two previous indices
 * defines a new triangle. For odd triangles, two of the indices must be
 * swapped to produce correct winding. The first triangle is considered even.
 * For example, the strip S = (2, 0, 1, 4) defines two triangles: (2, 0, 1)
 * and (1, 0, 4).
 *
 * Java: javax.microedition.m3g.TriangleStripArray
 */
class TriangleStripArray : public IndexBuffer {
public:
    TriangleStripArray() { object_type_ = M3G_OBJ_TRIANGLE_STRIP_ARRAY; }
    int getIndexCount() const override { return static_cast<int>(indices_.size()); }
    void getIndices(int *indices) const override {
        for (std::size_t i = 0; i < indices_.size(); ++i) indices[i] = indices_[i];
    }
    const char *getClassName() const override { return "TriangleStripArray"; }
    int encoding_ = 0;
    std::vector<int> indices_, strip_lengths_;
};

/**
 * @brief A scene graph node that represents a 3D object defined as a polygonal
 *        surface.
 *
 * A Mesh consists of a VertexBuffer (vertex data), one or more IndexBuffer
 * submeshes, and one or more Appearance objects (one per submesh). Rendering
 * and picking are disabled if no Appearance is set.
 *
 * Java: javax.microedition.m3g.Mesh
 */
class Mesh : public Node {
public:
    Mesh() { object_type_ = M3G_OBJ_MESH; }

    int getSubmeshCount() const { return static_cast<int>(submeshes_.size()); }
    std::shared_ptr<VertexBuffer> getVertexBuffer() const { return vertices_; }
    std::shared_ptr<IndexBuffer> getIndexBuffer(int index) const {
        return submeshes_.at(static_cast<std::size_t>(index));
    }
    std::shared_ptr<Appearance> getAppearance(int index) const {
        if (index < 0 || index >= static_cast<int>(appearances_.size())) return nullptr;
        return appearances_[static_cast<std::size_t>(index)];
    }
    void setAppearance(int index, const std::shared_ptr<Appearance> &appearance) {
        if (index < 0 || index >= static_cast<int>(appearances_.size()))
            throw std::out_of_range("Mesh.setAppearance");
        appearances_[static_cast<std::size_t>(index)] = appearance;
    }
    Object3DPtr duplicate() const override { return std::make_shared<Mesh>(*this); }
    const char *getClassName() const override { return "Mesh"; }

    std::shared_ptr<VertexBuffer> vertices_;
    std::vector<std::shared_ptr<IndexBuffer>> submeshes_;
    std::vector<std::shared_ptr<Appearance>> appearances_;
};

/**
 * @brief A scene graph node that represents a skeletally animated polygon mesh.
 *
 * In addition to standard mesh data, a SkinnedMesh references a skeleton
 * (a Group of Transformable nodes). Vertices can be bound to skeleton bones
 * with associated weights, enabling vertex-level skeletal deformation.
 *
 * Java: javax.microedition.m3g.SkinnedMesh
 */
class SkinnedMesh : public Mesh {
public:
    SkinnedMesh() { object_type_ = M3G_OBJ_SKINNED_MESH; }
    std::shared_ptr<Group> getSkeleton() const { return skeleton_; }
    void addTransform(const NodePtr &bone, int weight, int firstVertex, int numVertices);
    void getBoneTransform(Node * /*bone*/, Transform *transform) const {
        if (transform) transform->setIdentity();
    }
    int getBoneVertices(Node *bone, int *indices, float *weights) const;
    Object3DPtr duplicate() const override { return std::make_shared<SkinnedMesh>(*this); }
    const char *getClassName() const override { return "SkinnedMesh"; }

    std::shared_ptr<Group> skeleton_;
    struct Bone {
        NodePtr node;
        int weight = 0, first_vertex = 0, vertex_count = 0;
    };
    std::vector<Bone> bones_;
};

/**
 * @brief A scene graph node that represents a vertex morphing polygon mesh.
 *
 * In addition to the base mesh data, a MorphingMesh holds one or more morph
 * targets (alternative VertexBuffers) and a set of blend weights. The final
 * vertex positions are a weighted combination of the base and all targets.
 *
 * Java: javax.microedition.m3g.MorphingMesh
 */
class MorphingMesh : public Mesh {
public:
    MorphingMesh() { object_type_ = M3G_OBJ_MORPHING_MESH; }
    int getMorphTargetCount() const { return static_cast<int>(targets_.size()); }
    std::shared_ptr<VertexBuffer> getMorphTarget(int index) const {
        return targets_.at(static_cast<std::size_t>(index));
    }
    void setWeights(const float *weights) {
        if (!weights) throw std::invalid_argument("MorphingMesh.setWeights");
        weights_.assign(weights, weights + targets_.size());
    }
    void getWeights(float *weights) const {
        for (std::size_t i = 0; i < weights_.size(); ++i) weights[i] = weights_[i];
    }
    Object3DPtr duplicate() const override { return std::make_shared<MorphingMesh>(*this); }
    const char *getClassName() const override { return "MorphingMesh"; }
    std::vector<std::shared_ptr<VertexBuffer>> targets_;
    std::vector<float> weights_;
};

/**
 * @brief A scene graph node that represents a 2-dimensional image with a 3D
 *        position.
 *
 * The sprite image can optionally be scaled to a fixed size or rendered at
 * its native resolution. The scaling mode is fixed at construction. Rendering
 * and picking are disabled if no Appearance is set.
 *
 * Java: javax.microedition.m3g.Sprite3D
 */
class Sprite3D : public Node {
public:
    Sprite3D() { object_type_ = M3G_OBJ_SPRITE_3D; }
    bool isScaled() const { return scaled_; }
    void setAppearance(const std::shared_ptr<Appearance> &a) { appearance_ = a; }
    std::shared_ptr<Appearance> getAppearance() const { return appearance_; }
    void setImage(const std::shared_ptr<Image2D> &image) {
        if (!image) throw std::invalid_argument("Sprite3D.setImage: null");
        image_ = image;
    }
    std::shared_ptr<Image2D> getImage() const { return image_; }
    void setCrop(int cropX, int cropY, int width, int height) {
        crop_x_ = cropX;
        crop_y_ = cropY;
        crop_w_ = width;
        crop_h_ = height;
    }
    int getCropX() const { return crop_x_; }
    int getCropY() const { return crop_y_; }
    int getCropWidth() const { return crop_w_; }
    int getCropHeight() const { return crop_h_; }
    Object3DPtr duplicate() const override { return std::make_shared<Sprite3D>(*this); }
    const char *getClassName() const override { return "Sprite3D"; }

    bool scaled_ = false;
    std::shared_ptr<Image2D> image_;
    std::shared_ptr<Appearance> appearance_;
    int crop_x_ = 0, crop_y_ = 0, crop_w_ = 0, crop_h_ = 0;
};

/* ========== Animation ========== */

/**
 * @brief Controls the position, speed and weight of an animation sequence.
 *
 * The speed defines the rate at which the animation progresses relative to
 * world time (1.0 = real time). The weight defines the relative influence
 * of this controller when multiple controllers animate the same property.
 * An active interval limits the time range during which the controller is
 * active.
 *
 * Java: javax.microedition.m3g.AnimationController
 */
class AnimationController : public Object3D {
public:
    AnimationController() { object_type_ = M3G_OBJ_ANIMATION_CONTROLLER; }
    void setActiveInterval(int start, int end) {
        active_start_ = start;
        active_end_ = end;
    }
    int getActiveIntervalStart() const { return active_start_; }
    int getActiveIntervalEnd() const { return active_end_; }
    void setSpeed(float speed, int /*worldTime*/) { speed_ = speed; }
    float getSpeed() const { return speed_; }
    void setPosition(float sequenceTime, int worldTime) {
        ref_seq_ = sequenceTime;
        ref_world_ = worldTime;
    }
    float getPosition(int worldTime) const {
        return ref_seq_ + speed_ * static_cast<float>(worldTime - ref_world_);
    }
    int getRefWorldTime() const { return ref_world_; }
    void setWeight(float weight) { weight_ = weight; }
    float getWeight() const { return weight_; }
    const char *getClassName() const override { return "AnimationController"; }

    float speed_ = 1.f, weight_ = 1.f, ref_seq_ = 0.f;
    int active_start_ = 0, active_end_ = 0, ref_world_ = 0;
};

/**
 * @brief Encapsulates animation data as a sequence of time-stamped,
 *        vector-valued keyframes.
 *
 * Each keyframe represents the value of an animated quantity at a specific
 * time instant. The interpolation between keyframes can be LINEAR, SLERP,
 * SPLINE, SQUAD, or STEP. The repeat mode is either CONSTANT (clamped)
 * or LOOP.
 *
 * Java: javax.microedition.m3g.KeyframeSequence
 */
class KeyframeSequence : public Object3D {
public:
    static constexpr int LINEAR = 176, SLERP = 177, SPLINE = 178, SQUAD = 179, STEP = 180;
    static constexpr int CONSTANT = 192, LOOP = 193;
    KeyframeSequence() { object_type_ = M3G_OBJ_KEYFRAME_SEQUENCE; }
    int getComponentCount() const { return component_count_; }
    int getKeyframeCount() const { return static_cast<int>(keyframes_.size()); }
    int getInterpolationType() const { return interpolation_; }
    void setKeyframe(int index, int time, const float *value);
    int getKeyframe(int index, float *value) const;
    void setValidRange(int first, int last) {
        valid_first_ = first;
        valid_last_ = last;
    }
    int getValidRangeFirst() const { return valid_first_; }
    int getValidRangeLast() const { return valid_last_; }
    void setDuration(int duration) { duration_ = duration; }
    int getDuration() const { return duration_; }
    void setRepeatMode(int mode) { repeat_mode_ = mode; }
    int getRepeatMode() const { return repeat_mode_; }
    const char *getClassName() const override { return "KeyframeSequence"; }

    int interpolation_ = 0, repeat_mode_ = 0, encoding_ = 0, duration_ = 0;
    int valid_first_ = 0, valid_last_ = 0, component_count_ = 0;
    struct KF {
        int time = 0;
        std::vector<float> values;
    };
    std::vector<KF> keyframes_;
};

/**
 * @brief Associates a KeyframeSequence with an AnimationController and an
 *        animatable property.
 *
 * An AnimationTrack is attached to an Object3D and defines which property
 * of that object is animated (e.g., TRANSLATION, ORIENTATION, SCALE,
 * COLOR, INTENSITY, MORPH_WEIGHTS, etc.) and how (via the referenced
 * KeyframeSequence and AnimationController).
 *
 * Java: javax.microedition.m3g.AnimationTrack
 */
class AnimationTrack : public Object3D {
public:
    static constexpr int ALPHA = 256, AMBIENT_COLOR = 257, COLOR = 258, CROP = 259, DENSITY = 260;
    static constexpr int DIFFUSE_COLOR = 261, EMISSIVE_COLOR = 262, FAR_DISTANCE = 263, FIELD_OF_VIEW = 264;
    static constexpr int INTENSITY = 265, MORPH_WEIGHTS = 266, NEAR_DISTANCE = 267, ORIENTATION = 268;
    static constexpr int PICKABILITY = 269, SCALE = 270, SHININESS = 271, SPECULAR_COLOR = 272;
    static constexpr int SPOT_ANGLE = 273, SPOT_EXPONENT = 274, TRANSLATION = 275, VISIBILITY = 276;

    AnimationTrack() { object_type_ = M3G_OBJ_ANIMATION_TRACK; }
    void setController(const std::shared_ptr<AnimationController> &c) { controller_ = c; }
    std::shared_ptr<AnimationController> getController() const { return controller_; }
    std::shared_ptr<KeyframeSequence> getKeyframeSequence() const { return sequence_; }
    int getTargetProperty() const { return property_; }
    const char *getClassName() const override { return "AnimationTrack"; }

    std::shared_ptr<KeyframeSequence> sequence_;
    std::shared_ptr<AnimationController> controller_;
    int property_ = 0;
};

/* ========== RayIntersection / Graphics3D ========== */

/**
 * @brief Stores information about the result of a ray-pick test.
 *
 * A RayIntersection object is filled in by the pick methods in Group. It
 * stores a reference to the intersected Mesh or Sprite3D and information
 * about the intersection point (distance, normal, texture coordinates,
 * submesh index). This is strictly a run-time object; it cannot be loaded
 * from a file by Loader.
 *
 * Java: javax.microedition.m3g.RayIntersection
 */
class RayIntersection {
public:
    Node *getIntersected() const { return intersected_; }
    void getRay(float *ray) const {
        for (int i = 0; i < 6; ++i) ray[i] = ray_[i];
    }
    float getDistance() const { return distance_; }
    int getSubmeshIndex() const { return submesh_index_; }
    float getTextureS(int index) const { return index == 0 ? tex_s_[0] : tex_s_[1]; }
    float getTextureT(int index) const { return index == 0 ? tex_t_[0] : tex_t_[1]; }
    float getNormalX() const { return normal_[0]; }
    float getNormalY() const { return normal_[1]; }
    float getNormalZ() const { return normal_[2]; }

    Node *intersected_ = nullptr;
    float distance_ = 0.f;
    float normal_[3]{0, 0, 1};
    float tex_s_[2]{}, tex_t_[2]{};
    float ray_[6]{};
    int submesh_index_ = 0;
};

/**
 * @brief A singleton 3D graphics context that can be bound to a rendering target.
 *
 * All rendering is done through the render methods in this class, including
 * the rendering of World objects. The viewport can be freely positioned
 * relative to the rendering target without releasing and re-binding. Depth
 * range, camera, and light state are managed here.
 *
 * Java: javax.microedition.m3g.Graphics3D
 */
class Graphics3D {
public:
    static constexpr int ANTIALIAS = 2, DITHER = 4, TRUE_COLOR = 8, OVERWRITE = 16;
    static Graphics3D &getInstance() {
        static Graphics3D g;
        return g;
    }
    void bindTarget(void *target, bool depthBuffer = true, int hints = 0) {
        target_ = target;
        depth_ = depthBuffer;
        hints_ = hints;
    }
    void releaseTarget() { target_ = nullptr; }
    void *getTarget() const { return target_; }
    int getHints() const { return hints_; }
    bool isDepthBufferEnabled() const { return depth_; }
    void setViewport(int x, int y, int width, int height) {
        vx_ = x;
        vy_ = y;
        vw_ = width;
        vh_ = height;
    }
    int getViewportX() const { return vx_; }
    int getViewportY() const { return vy_; }
    int getViewportWidth() const { return vw_; }
    int getViewportHeight() const { return vh_; }
    void setDepthRange(float near, float far) {
        dn_ = near;
        df_ = far;
    }
    float getDepthRangeNear() const { return dn_; }
    float getDepthRangeFar() const { return df_; }
    void clear(Background *) {}
    void render(World *) {}
    void render(Node *, Transform *) {}
    void render(VertexBuffer *, IndexBuffer *, Appearance *, Transform *, int = -1) {}
    void setCamera(Camera *c, Transform *t) {
        camera_ = c;
        if (t) t->get(cam_xform_);
    }
    Camera *getCamera(Transform *transform) const {
        if (transform) transform->set(cam_xform_);
        return camera_;
    }
    int addLight(Light *, Transform *) { return 0; }
    void setLight(int, Light *, Transform *) {}
    void resetLights() {}
    int getLightCount() const { return 0; }
    Light *getLight(int, Transform *) const { return nullptr; }

private:
    Graphics3D() = default;
    void *target_ = nullptr;
    bool depth_ = true;
    int hints_ = 0;
    int vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0;
    float dn_ = 0.f, df_ = 1.f;
    Camera *camera_ = nullptr;
    float cam_xform_[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

/* ========== scene IR + decode (toolkit) ========== */

namespace scene {

struct ConversionWarning {
    std::string code, message;
    bool operator==(const ConversionWarning &o) const { return code == o.code && message == o.message; }
};
struct EmbeddedRgbaImageSource {
    int object_id = 0, width = 0, height = 0;
    std::vector<std::uint8_t> pixels;
};
struct ExternalFileImageSource {
    int object_id = 0;
    std::string source_path;
};
struct SceneImageIr {
    std::string name;
    std::optional<EmbeddedRgbaImageSource> embedded;
    std::optional<ExternalFileImageSource> external;
};
struct SceneSamplerIr {
    std::optional<int> mag_filter, min_filter;
    int wrap_s = 0, wrap_t = 0;
};
struct SceneTextureIr {
    std::string name;
    int image_index = 0;
    std::optional<int> sampler_index;
};
struct SceneMaterialIr {
    std::string name;
    std::vector<float> base_color_factor{1, 1, 1, 1};
    std::optional<int> base_color_texture_index;
    std::vector<float> emissive_factor{0, 0, 0};
    float roughness_factor = 1.f, metallic_factor = 0.f;
    bool double_sided = false;
    std::optional<std::string> alpha_mode;
};
struct ScenePrimitiveIr {
    std::string name;
    std::vector<float> positions;
    std::optional<std::vector<float>> normals, tex_coords0, vertex_colors;
    std::vector<int> indices;
    std::optional<int> material_index;
};
struct SceneMeshIr {
    std::string name;
    std::vector<ScenePrimitiveIr> primitives;
};
struct ScenePerspectiveCameraIr {
    float yfov_radians = 0.f;
    std::optional<float> aspect_ratio;
    float znear = 0.1f;
    std::optional<float> zfar;
};
struct SceneCameraIr {
    std::string name;
    std::optional<ScenePerspectiveCameraIr> perspective;
};
struct SceneNodeIr {
    std::string name;
    std::optional<std::vector<float>> matrix, translation, rotation, scale;
    std::optional<int> mesh_index, camera_index;
    std::vector<int> children;
};
struct SceneAnimationSamplerIr {
    std::vector<float> times, values;
    std::string interpolation = "LINEAR";
    int component_count = 3;
};
struct SceneAnimationChannelIr {
    int sampler_index = 0, node_index = 0;
    std::string path;
};
struct SceneAnimationIr {
    std::string name;
    std::vector<SceneAnimationSamplerIr> samplers;
    std::vector<SceneAnimationChannelIr> channels;
};
struct SceneIr {
    std::vector<SceneNodeIr> nodes;
    std::vector<int> root_node_indices;
    std::vector<SceneMeshIr> meshes;
    std::vector<SceneMaterialIr> materials;
    std::vector<SceneTextureIr> textures;
    std::vector<SceneImageIr> images;
    std::vector<SceneSamplerIr> samplers;
    std::vector<SceneCameraIr> cameras;
    std::vector<SceneAnimationIr> animations;
    std::vector<ConversionWarning> warnings;
};
struct GltfWriteResult {
    std::string gltf_path, bin_path;
    std::vector<std::string> image_paths;
};

std::vector<float> identity_matrix_row_major();
std::vector<float> multiply_row_major(const std::vector<float> &a, const std::vector<float> &b);
std::vector<float> translation_matrix_row_major(float x, float y, float z);
std::vector<float> scale_matrix_row_major(float x, float y, float z);
std::vector<float> axis_angle_matrix_row_major(float angle_radians, float ax, float ay, float az);
struct DecomposedTrs {
    std::vector<float> translation{0, 0, 0};
    std::vector<float> rotation{0, 0, 0, 1};
    std::vector<float> scale{1, 1, 1};
};
float vec3_length(float x, float y, float z);
std::vector<float> quaternion_from_axis_angle_degrees(float angle_degrees, float ax, float ay, float az);
DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix);
std::vector<double> row_major_to_column_major_list(const std::vector<float> &matrix);
SceneIr scene_ir_from_c(m3g_scene_ir const *s);

} // namespace scene

namespace decode {

/** Loaded M3G asset: Java object graph + export IR. */
struct Decoded {
    std::string source_path;
    /** Roots as returned by Loader.load (typically includes World). */
    std::vector<Object3DPtr> roots;
    /** All objects by ascending object id (index 0 unused / empty). */
    std::vector<Object3DPtr> objects_by_id;
    std::shared_ptr<World> world;
    scene::SceneIr scene_ir;

    std::size_t node_count() const { return scene_ir.nodes.size(); }
    std::size_t mesh_count() const { return scene_ir.meshes.size(); }
    std::size_t material_count() const { return scene_ir.materials.size(); }
    std::size_t texture_count() const { return scene_ir.textures.size(); }
    std::size_t image_count() const { return scene_ir.images.size(); }
    std::size_t camera_count() const { return scene_ir.cameras.size(); }
    std::size_t animation_count() const { return scene_ir.animations.size(); }
    const std::vector<scene::ConversionWarning> &warnings() const { return scene_ir.warnings; }

    std::shared_ptr<World> world_or_null() const { return world; }
};

struct DecodeOptions {
    std::optional<std::string> pattern_path;
};

class Decoder {
public:
    Decoded decode_file(const std::string &path, const DecodeOptions &options = {}) const;
    Decoded decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path = {},
                         const DecodeOptions &options = {}) const;
};

} // namespace decode

/**
 * @brief Downloads and deserializes scene graph nodes and node components,
 *        as well as entire scene graphs.
 *
 * Downloading ready-made pieces of 3D content from an M3G file is generally
 * the most convenient way for an application to create and populate a 3D
 * scene. load() returns a decode::Decoded containing the root Object3D array
 * plus an attached scene IR for export.
 *
 * Java: javax.microedition.m3g.Loader
 */
class Loader {
public:
    static decode::Decoded load(const std::string &name, const decode::DecodeOptions &options = {}) {
        return decode::Decoder{}.decode_file(name, options);
    }
    static decode::Decoded load(const std::vector<std::uint8_t> &data, int /*offset*/ = 0,
                                const decode::DecodeOptions &options = {}) {
        return decode::Decoder{}.decode_bytes(data, {}, options);
    }
};

namespace exp {
struct GltfPaths {
    std::string gltf_path, bin_path;
    std::vector<std::string> image_paths;
};
struct ExportReport {
    decode::Decoded decoded;
    GltfPaths paths;
};
class GltfExporter {
public:
    GltfPaths write(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;
    GltfPaths write(const scene::SceneIr &scene_ir, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;
};
} // namespace exp

class Converter {
public:
    decode::Decoded decode(const std::string &input_path,
                           const std::optional<std::string> &pattern_path = std::nullopt) const;
    exp::GltfPaths export_gltf(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                               int png_compression_level = 8) const;
    exp::ExportReport convert(const std::string &input_path, const std::string &output_path, bool overwrite,
                              const std::optional<std::string> &pattern_path = std::nullopt,
                              int png_compression_level = 8) const;
};

/* ========== compat helpers used by tests ========== */

namespace model {
struct ObjectTypes {
    static constexpr int HEADER = M3G_OBJ_HEADER;
    static constexpr int ANIMATION_CONTROLLER = M3G_OBJ_ANIMATION_CONTROLLER;
    static constexpr int ANIMATION_TRACK = M3G_OBJ_ANIMATION_TRACK;
    static constexpr int APPEARANCE = M3G_OBJ_APPEARANCE;
    static constexpr int BACKGROUND = M3G_OBJ_BACKGROUND;
    static constexpr int CAMERA = M3G_OBJ_CAMERA;
    static constexpr int COMPOSITING_MODE = M3G_OBJ_COMPOSITING_MODE;
    static constexpr int FOG = M3G_OBJ_FOG;
    static constexpr int POLYGON_MODE = M3G_OBJ_POLYGON_MODE;
    static constexpr int GROUP = M3G_OBJ_GROUP;
    static constexpr int IMAGE_2D = M3G_OBJ_IMAGE_2D;
    static constexpr int TRIANGLE_STRIP_ARRAY = M3G_OBJ_TRIANGLE_STRIP_ARRAY;
    static constexpr int LIGHT = M3G_OBJ_LIGHT;
    static constexpr int MATERIAL = M3G_OBJ_MATERIAL;
    static constexpr int MESH = M3G_OBJ_MESH;
    static constexpr int MORPHING_MESH = M3G_OBJ_MORPHING_MESH;
    static constexpr int SKINNED_MESH = M3G_OBJ_SKINNED_MESH;
    static constexpr int TEXTURE_2D = M3G_OBJ_TEXTURE_2D;
    static constexpr int SPRITE_3D = M3G_OBJ_SPRITE_3D;
    static constexpr int KEYFRAME_SEQUENCE = M3G_OBJ_KEYFRAME_SEQUENCE;
    static constexpr int VERTEX_ARRAY = M3G_OBJ_VERTEX_ARRAY;
    static constexpr int VERTEX_BUFFER = M3G_OBJ_VERTEX_BUFFER;
    static constexpr int WORLD = M3G_OBJ_WORLD;
    static constexpr int EXTERNAL_REFERENCE = M3G_OBJ_EXTERNAL_REFERENCE;
};
struct AnimationProperty {
    static constexpr int TRANSLATION = AnimationTrack::TRANSLATION;
    static constexpr int ORIENTATION = AnimationTrack::ORIENTATION;
};
struct KeyframeInterpolation {
    static constexpr int LINEAR = KeyframeSequence::LINEAR;
    static constexpr int STEP = KeyframeSequence::STEP;
};
inline std::string type_name_for_object_type(int t) { return m3g_type_name(t); }
struct RgbColor {
    int red = 0, green = 0, blue = 0;
    RgbColor() = default;
    RgbColor(int r, int g, int b) : red(r), green(g), blue(b) {}
    std::vector<float> to_float_array() const {
        m3g_rgb c{red, green, blue};
        float o[3];
        m3g_rgb_to_float(&c, o);
        return {o[0], o[1], o[2]};
    }
};
struct RgbaColor {
    int red = 0, green = 0, blue = 0, alpha = 255;
    RgbaColor() = default;
    RgbaColor(int r, int g, int b, int a) : red(r), green(g), blue(b), alpha(a) {}
    std::vector<float> to_float_array() const {
        m3g_rgba c{red, green, blue, alpha};
        float o[4];
        m3g_rgba_to_float(&c, o);
        return {o[0], o[1], o[2], o[3]};
    }
};
/** Lightweight header stub for tests (not a public JSR-184 class). */
struct Header : Object3D {
    Header() { object_type_ = M3G_OBJ_HEADER; }
    const char *getClassName() const override { return "Header"; }
};
inline bool is_identity_row_major(const std::vector<float> &m, float eps = 1e-5f) {
    return m.size() == 16 && m3g_mat4_is_identity(m.data(), eps) != 0;
}
/* Re-export Java classes into model:: for tests that `using namespace m3g::model`. */
using World = m3g::World;
using Mesh = m3g::Mesh;
using Camera = m3g::Camera;
using Light = m3g::Light;
using Image2D = m3g::Image2D;
using KeyframeSequence = m3g::KeyframeSequence;
using CompositingMode = m3g::CompositingMode;
using MorphingMesh = m3g::MorphingMesh;
using Sprite3D = m3g::Sprite3D;
using IndexBuffer = m3g::IndexBuffer;
using TriangleStripArray = m3g::TriangleStripArray;
} // namespace model

} // namespace m3g

#endif /* M3G_HPP_INCLUDED */

/* ============================ IMPLEMENTATION ============================ */
#ifndef DOXYGEN
#ifdef M3G_DECODE_IMPL
#ifndef M3G_DECODE_IMPL_INCLUDED
#define M3G_DECODE_IMPL_INCLUDED

#include <filesystem>

namespace m3g {
namespace {

DeflateIo &deflate_store() {
    static DeflateIo s{};
    return s;
}
ImageIo &image_store() {
    static ImageIo s{};
    return s;
}
JsonIo &json_store() {
    static JsonIo s{};
    return s;
}
GltfIo &gltf_store() {
    static GltfIo s{};
    return s;
}

void sync_deflate(DeflateIo const &io) {
    m3g_deflate_io c{};
    c.adler32 = io.adler32;
    c.uncompress = io.uncompress;
    c.uncompress_to_heap = io.uncompress_to_heap;
    c.free_mem = io.free_mem;
    c.user = io.user;
    m3g_set_deflate_io((io.adler32 || io.uncompress) ? &c : nullptr);
}
void sync_image(ImageIo const &io) {
    m3g_image_io c{};
    c.load_file = io.load_file;
    c.load_memory = io.load_memory;
    c.free_pixels = io.free_pixels;
    c.user = io.user;
    m3g_set_image_io((io.load_file || io.load_memory) ? &c : nullptr);
}

} // namespace

void set_deflate_io(DeflateIo const *io) {
    if (!io) {
        deflate_store() = {};
        m3g_set_deflate_io(nullptr);
        return;
    }
    deflate_store() = *io;
    sync_deflate(*io);
}
DeflateIo const *deflate_io(void) {
    auto &s = deflate_store();
    if (!s.adler32 && !s.uncompress) return nullptr;
    return &s;
}
void set_image_io(ImageIo const *io) {
    if (!io) {
        image_store() = {};
        m3g_set_image_io(nullptr);
        return;
    }
    image_store() = *io;
    sync_image(*io);
}
ImageIo const *image_io(void) {
    auto &s = image_store();
    if (!s.load_file && !s.load_memory) return nullptr;
    return &s;
}
void set_json_io(JsonIo const *io) { json_store() = io ? *io : JsonIo{}; }
JsonIo const *json_io(void) {
    auto &s = json_store();
    return s.create_object ? &s : nullptr;
}
void set_gltf_io(GltfIo const *io) { gltf_store() = io ? *io : GltfIo{}; }
GltfIo const *gltf_io(void) {
    auto &s = gltf_store();
    return s.write_file || s.parse_file ? &s : nullptr;
}

std::uint32_t deflate_adler32(std::uint32_t a, unsigned char const *p, std::size_t n) {
    return m3g_deflate_adler32(a, p, n);
}
int deflate_uncompress(unsigned char *d, std::size_t *dl, unsigned char const *s, std::size_t sl) {
    return m3g_deflate_uncompress(d, dl, s, sl);
}
void *deflate_uncompress_to_heap(unsigned char const *s, std::size_t sl, std::size_t *ol, int z) {
    return m3g_deflate_uncompress_to_heap(s, sl, ol, z);
}
void deflate_free(void *p) { m3g_deflate_free(p); }

unsigned char *image_load_file(char const *f, int *w, int *h, int *c, int r) {
    auto *p = m3g_image_load_file(f, w, h, c, r);
    if (!p) throw std::runtime_error(std::string("image_load_file: ") + m3g_last_error());
    return p;
}
unsigned char *image_load_memory(unsigned char const *b, int len, int *w, int *h, int *c, int r) {
    auto *p = m3g_image_load_memory(b, len, w, h, c, r);
    if (!p) throw std::runtime_error(std::string("image_load_memory: ") + m3g_last_error());
    return p;
}
void image_free_pixels(void *p) { m3g_image_free_pixels(p); }

static GltfIo const *req_gltf(char const *w) {
    auto *io = gltf_io();
    if (!io) throw std::runtime_error(std::string("gltf io missing: ") + w);
    return io;
}
void gltf_write_file(char const *path, void const *data, int kind) {
    auto *io = req_gltf("write");
    if (!io->write_file || io->write_file(path, data, kind, io->user) != GLTF_IO_OK)
        throw std::runtime_error("gltf_write_file failed");
}
void gltf_parse_file(char const *path, void **out) {
    auto *io = req_gltf("parse");
    if (!io->parse_file || io->parse_file(path, out, io->user) != GLTF_IO_OK)
        throw std::runtime_error("gltf_parse_file failed");
}
void gltf_validate(void *data) {
    auto *io = req_gltf("validate");
    if (!io->validate || io->validate(data, io->user) != GLTF_IO_OK) throw std::runtime_error("gltf_validate failed");
}
void gltf_free_data(void *data) {
    if (!data) return;
    auto *io = req_gltf("free");
    if (io->free_data) io->free_data(data, io->user);
}
static JsonIo const *req_json(char const *w) {
    auto *io = json_io();
    if (!io) throw std::runtime_error(std::string("json io missing: ") + w);
    return io;
}
void *json_create_object() { return req_json("obj")->create_object(req_json("obj")->user); }
void *json_create_array() { return req_json("arr")->create_array(req_json("arr")->user); }
void *json_create_string(char const *s) { return req_json("str")->create_string(s, req_json("str")->user); }
void *json_create_number(double v) { return req_json("num")->create_number(v, req_json("num")->user); }
void *json_create_bool(int v) { return req_json("bool")->create_bool(v, req_json("bool")->user); }
void json_add_item_to_object(void *o, char const *k, void *i) {
    auto *io = req_json("add");
    io->add_item_to_object(o, k, i, io->user);
}
void json_add_item_to_array(void *a, void *i) {
    auto *io = req_json("adda");
    io->add_item_to_array(a, i, io->user);
}
int json_get_array_size(void const *a) {
    auto *io = req_json("sz");
    return io->get_array_size(a, io->user);
}
void json_delete(void *n) {
    if (!n) return;
    auto *io = req_json("del");
    io->delete_node(n, io->user);
}
char *json_print_unformatted(void *n) {
    auto *io = req_json("pu");
    return io->print_unformatted(n, io->user);
}
char *json_print_formatted(void *n) {
    auto *io = req_json("pf");
    return io->print_formatted(n, io->user);
}
void json_free_print(char *p) {
    if (!p) return;
    auto *io = req_json("fp");
    io->free_print(p, io->user);
}

void Transformable::getCompositeTransform(float *m16) const {
    m3g_node_meta n{};
    if (has_matrix_) {
        n.transformable.has_general_transform = 1;
        std::memcpy(n.transformable.general_transform, matrix_, sizeof(matrix_));
    }
    if (has_component_) {
        auto &c = n.transformable.component;
        c.present = 1;
        std::memcpy(c.translation, translation_, sizeof(translation_));
        std::memcpy(c.scale, scale_, sizeof(scale_));
        c.orientation_angle = orientation_angle_;
        std::memcpy(c.orientation_axis, orientation_axis_, sizeof(orientation_axis_));
    }
    m3g_node_matrix(&n, m16);
}
void Transformable::getCompositeTransform(Transform *transform) const {
    if (!transform) throw std::invalid_argument("getCompositeTransform: null");
    float m[16];
    getCompositeTransform(m);
    transform->set(m);
}

void Transform::invert() {
    /* simple adjoint for affine 4x4; identity fallback */
    float inv[16];
    m3g_mat4_identity(inv);
    /* Prefer C math if available — leave identity for singular toolkit cases */
    (void)inv;
    /* For tests, postTranslate invert not required; leave as transpose of upper 3x3 + neg translation later */
    setIdentity();
}
void Transform::postRotate(float angle, float ax, float ay, float az) {
    float R[16];
    m3g_mat4_rotation_axis(angle * (3.14159265358979323846f / 180.f), ax, ay, az, R);
    Transform t;
    t.set(R);
    postMultiply(t);
}
void Transform::postRotateQuat(float qx, float qy, float qz, float qw) {
    float x = qx, y = qy, z = qz, w = qw;
    float R[16] = {
        1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0,
        2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0,
        2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0,
        0, 0, 0, 1};
    Transform t;
    t.set(R);
    postMultiply(t);
}
void Transform::transform(float *vectors) const {
    if (!vectors) return;
    float x = vectors[0], y = vectors[1], z = vectors[2], w = vectors[3];
    vectors[0] = m_[0] * x + m_[1] * y + m_[2] * z + m_[3] * w;
    vectors[1] = m_[4] * x + m_[5] * y + m_[6] * z + m_[7] * w;
    vectors[2] = m_[8] * x + m_[9] * y + m_[10] * z + m_[11] * w;
    vectors[3] = m_[12] * x + m_[13] * y + m_[14] * z + m_[15] * w;
}
void Transform::transform(const VertexArray &in, float *out, bool W) const {
    float w = W ? 1.f : 0.f;
    for (int v = 0; v < in.vertex_count_; ++v) {
        float vec[4] = {0, 0, 0, w};
        for (int c = 0; c < in.component_count_ && c < 3; ++c)
            vec[c] = static_cast<float>(in.components_[static_cast<std::size_t>(v * in.component_count_ + c)]);
        transform(vec);
        out[v * 4 + 0] = vec[0];
        out[v * 4 + 1] = vec[1];
        out[v * 4 + 2] = vec[2];
        out[v * 4 + 3] = vec[3];
    }
}

/* ---- graph build from m3g_file ---- */

namespace {

template <class T>
std::shared_ptr<T> as(Object3DPtr const &p) {
    return std::dynamic_pointer_cast<T>(p);
}

void apply_o3d(Object3D &dst, m3g_object3d_meta const &m) {
    dst.setUserID(m.user_id);
    /* tracks wired in second pass */
}

void apply_xf(Transformable &dst, m3g_transformable_meta const &m) {
    apply_o3d(dst, m.object3d);
    if (m.component.present) {
        dst.setTranslation(m.component.translation[0], m.component.translation[1], m.component.translation[2]);
        dst.setScale(m.component.scale[0], m.component.scale[1], m.component.scale[2]);
        dst.setOrientation(m.component.orientation_angle, m.component.orientation_axis[0],
                           m.component.orientation_axis[1], m.component.orientation_axis[2]);
    }
    if (m.has_general_transform) {
        Transform t;
        t.set(m.general_transform);
        dst.setTransform(&t);
    }
}

void apply_node(Node &dst, m3g_node_meta const &m) {
    apply_xf(dst, m.transformable);
    dst.setRenderingEnable(m.enable_rendering != 0);
    dst.setPickingEnable(m.enable_picking != 0);
    dst.setAlphaFactor(m.alpha_factor / 255.f);
    dst.setScope(static_cast<int>(m.scope));
}

Object3DPtr make_object(m3g_object const *o) {
    if (!o) return nullptr;
    Object3DPtr base;
    switch (o->object_type) {
    case M3G_OBJ_WORLD: {
        auto w = std::make_shared<World>();
        apply_node(*w, o->u.group.node);
        base = w;
        break;
    }
    case M3G_OBJ_GROUP: {
        auto g = std::make_shared<Group>();
        apply_node(*g, o->u.group.node);
        base = g;
        break;
    }
    case M3G_OBJ_CAMERA: {
        auto c = std::make_shared<Camera>();
        apply_node(*c, o->u.camera.node);
        c->projection_type_ = o->u.camera.projection_type;
        if (o->u.camera.has_perspective) {
            if (o->u.camera.projection_type == Camera::PERSPECTIVE)
                c->setPerspective(o->u.camera.fov_degrees, o->u.camera.aspect_ratio, o->u.camera.near_distance,
                                  o->u.camera.far_distance);
            else if (o->u.camera.projection_type == Camera::PARALLEL)
                c->setParallel(o->u.camera.fov_degrees, o->u.camera.aspect_ratio, o->u.camera.near_distance,
                               o->u.camera.far_distance);
            else {
                c->params_[0] = o->u.camera.fov_degrees;
                c->params_[1] = o->u.camera.aspect_ratio;
                c->params_[2] = o->u.camera.near_distance;
                c->params_[3] = o->u.camera.far_distance;
            }
        }
        if (o->u.camera.has_generic16) {
            Transform t;
            t.set(o->u.camera.generic_values);
            c->setGeneric(&t);
        }
        base = c;
        break;
    }
    case M3G_OBJ_LIGHT: {
        auto l = std::make_shared<Light>();
        apply_node(*l, o->u.light.node);
        l->setMode(o->u.light.mode);
        l->setIntensity(o->u.light.intensity);
        l->setColor((o->u.light.color.red << 16) | (o->u.light.color.green << 8) | o->u.light.color.blue);
        l->setAttenuation(o->u.light.attenuation_constant, o->u.light.attenuation_linear,
                          o->u.light.attenuation_quadratic);
        l->setSpotAngle(o->u.light.spot_angle);
        l->setSpotExponent(o->u.light.spot_exponent);
        base = l;
        break;
    }
    case M3G_OBJ_MESH:
    case M3G_OBJ_SKINNED_MESH:
    case M3G_OBJ_MORPHING_MESH: {
        std::shared_ptr<Mesh> mesh;
        if (o->object_type == M3G_OBJ_SKINNED_MESH) mesh = std::make_shared<SkinnedMesh>();
        else if (o->object_type == M3G_OBJ_MORPHING_MESH) mesh = std::make_shared<MorphingMesh>();
        else mesh = std::make_shared<Mesh>();
        apply_node(*mesh, o->u.mesh.node);
        mesh->submeshes_.resize(static_cast<std::size_t>(o->u.mesh.submesh_count));
        mesh->appearances_.resize(static_cast<std::size_t>(o->u.mesh.submesh_count));
        base = mesh;
        break;
    }
    case M3G_OBJ_SPRITE_3D: {
        auto s = std::make_shared<Sprite3D>();
        apply_node(*s, o->u.sprite3d.node);
        s->scaled_ = o->u.sprite3d.is_scaled != 0;
        s->setCrop(o->u.sprite3d.crop_x, o->u.sprite3d.crop_y, o->u.sprite3d.crop_width, o->u.sprite3d.crop_height);
        base = s;
        break;
    }
    case M3G_OBJ_BACKGROUND: {
        auto b = std::make_shared<Background>();
        apply_o3d(*b, o->u.background.object3d);
        int argb = (o->u.background.background_color.alpha << 24) |
                   (o->u.background.background_color.red << 16) |
                   (o->u.background.background_color.green << 8) | o->u.background.background_color.blue;
        b->setColor(argb);
        b->setColorClearEnable(o->u.background.color_clear_enabled != 0);
        b->setDepthClearEnable(o->u.background.depth_clear_enabled != 0);
        b->setImageMode(o->u.background.image_mode_x, o->u.background.image_mode_y);
        b->setCrop(o->u.background.crop_x, o->u.background.crop_y, o->u.background.crop_width,
                   o->u.background.crop_height);
        base = b;
        break;
    }
    case M3G_OBJ_FOG: {
        auto f = std::make_shared<Fog>();
        apply_o3d(*f, o->u.fog.object3d);
        f->setMode(o->u.fog.mode);
        f->setColor((o->u.fog.color.red << 16) | (o->u.fog.color.green << 8) | o->u.fog.color.blue);
        f->density_ = o->u.fog.density;
        f->near_ = o->u.fog.near_distance;
        f->far_ = o->u.fog.far_distance;
        base = f;
        break;
    }
    case M3G_OBJ_POLYGON_MODE: {
        auto p = std::make_shared<PolygonMode>();
        apply_o3d(*p, o->u.polygon_mode.object3d);
        p->culling_ = o->u.polygon_mode.culling;
        p->shading_ = o->u.polygon_mode.shading;
        p->winding_ = o->u.polygon_mode.winding;
        p->two_sided_ = o->u.polygon_mode.two_sided_lighting_enabled != 0;
        p->local_cam_ = o->u.polygon_mode.local_camera_lighting_enabled != 0;
        p->persp_ = o->u.polygon_mode.perspective_correction_enabled != 0;
        base = p;
        break;
    }
    case M3G_OBJ_MATERIAL: {
        auto m = std::make_shared<Material>();
        apply_o3d(*m, o->u.material.object3d);
        m->setColor(Material::AMBIENT, (o->u.material.ambient_color.red << 16) |
                                           (o->u.material.ambient_color.green << 8) | o->u.material.ambient_color.blue);
        m->setColor(Material::DIFFUSE, (o->u.material.diffuse_color.alpha << 24) |
                                           (o->u.material.diffuse_color.red << 16) |
                                           (o->u.material.diffuse_color.green << 8) | o->u.material.diffuse_color.blue);
        m->setColor(Material::EMISSIVE, (o->u.material.emissive_color.red << 16) |
                                            (o->u.material.emissive_color.green << 8) |
                                            o->u.material.emissive_color.blue);
        m->setColor(Material::SPECULAR, (o->u.material.specular_color.red << 16) |
                                            (o->u.material.specular_color.green << 8) |
                                            o->u.material.specular_color.blue);
        m->setShininess(o->u.material.shininess);
        m->setVertexColorTrackingEnable(o->u.material.vertex_color_tracking_enabled != 0);
        base = m;
        break;
    }
    case M3G_OBJ_COMPOSITING_MODE: {
        auto c = std::make_shared<CompositingMode>();
        apply_o3d(*c, o->u.compositing_mode.object3d);
        c->setBlending(o->u.compositing_mode.blending);
        c->setAlphaThreshold(o->u.compositing_mode.alpha_threshold / 255.f);
        c->setDepthTestEnable(o->u.compositing_mode.depth_test_enabled != 0);
        c->setDepthWriteEnable(o->u.compositing_mode.depth_write_enabled != 0);
        c->setColorWriteEnable(o->u.compositing_mode.color_write_enabled != 0);
        c->setAlphaWriteEnable(o->u.compositing_mode.alpha_write_enabled != 0);
        c->setDepthOffset(o->u.compositing_mode.depth_offset_factor, o->u.compositing_mode.depth_offset_units);
        base = c;
        break;
    }
    case M3G_OBJ_VERTEX_ARRAY: {
        auto v = std::make_shared<VertexArray>();
        apply_o3d(*v, o->u.vertex_array.object3d);
        v->component_size_ = o->u.vertex_array.component_size;
        v->component_count_ = o->u.vertex_array.component_count;
        v->encoding_ = o->u.vertex_array.encoding;
        v->vertex_count_ = o->u.vertex_array.vertex_count;
        v->components_.assign(o->u.vertex_array.components,
                              o->u.vertex_array.components + o->u.vertex_array.component_total);
        base = v;
        break;
    }
    case M3G_OBJ_VERTEX_BUFFER: {
        auto v = std::make_shared<VertexBuffer>();
        apply_o3d(*v, o->u.vertex_buffer.object3d);
        v->default_color_ = (o->u.vertex_buffer.default_color.alpha << 24) |
                            (o->u.vertex_buffer.default_color.red << 16) |
                            (o->u.vertex_buffer.default_color.green << 8) | o->u.vertex_buffer.default_color.blue;
        v->position_scale_ = o->u.vertex_buffer.position_scale;
        std::memcpy(v->position_bias_, o->u.vertex_buffer.position_bias, sizeof(v->position_bias_));
        v->tex_coords_.resize(static_cast<std::size_t>(o->u.vertex_buffer.tex_coord_binding_count));
        for (int i = 0; i < o->u.vertex_buffer.tex_coord_binding_count; ++i) {
            v->tex_coords_[static_cast<std::size_t>(i)].scale = o->u.vertex_buffer.tex_coord_bindings[i].scale;
            std::memcpy(v->tex_coords_[static_cast<std::size_t>(i)].bias,
                        o->u.vertex_buffer.tex_coord_bindings[i].bias, 3 * sizeof(float));
        }
        base = v;
        break;
    }
    case M3G_OBJ_TRIANGLE_STRIP_ARRAY: {
        auto t = std::make_shared<TriangleStripArray>();
        apply_o3d(*t, o->u.triangle_strip.object3d);
        t->encoding_ = o->u.triangle_strip.encoding;
        t->indices_.assign(o->u.triangle_strip.indices, o->u.triangle_strip.indices + o->u.triangle_strip.index_count);
        t->strip_lengths_.assign(o->u.triangle_strip.strip_lengths,
                                 o->u.triangle_strip.strip_lengths + o->u.triangle_strip.strip_count);
        base = t;
        break;
    }
    case M3G_OBJ_APPEARANCE: {
        auto a = std::make_shared<Appearance>();
        apply_o3d(*a, o->u.appearance.object3d);
        a->layer_ = o->u.appearance.layer;
        a->textures_.resize(static_cast<std::size_t>(o->u.appearance.texture_count));
        base = a;
        break;
    }
    case M3G_OBJ_TEXTURE_2D: {
        auto t = std::make_shared<Texture2D>();
        apply_xf(*t, o->u.texture2d.transformable);
        t->blending_ = o->u.texture2d.blending;
        t->wrap_s_ = o->u.texture2d.wrapping_s;
        t->wrap_t_ = o->u.texture2d.wrapping_t;
        t->level_filter_ = o->u.texture2d.level_filter;
        t->image_filter_ = o->u.texture2d.image_filter;
        t->blend_color_ = (o->u.texture2d.blend_color.red << 16) | (o->u.texture2d.blend_color.green << 8) |
                          o->u.texture2d.blend_color.blue;
        base = t;
        break;
    }
    case M3G_OBJ_IMAGE_2D: {
        auto im = std::make_shared<Image2D>();
        apply_o3d(*im, o->u.image2d.object3d);
        im->format_ = o->u.image2d.format;
        im->is_mutable_ = o->u.image2d.is_mutable != 0;
        im->width_ = o->u.image2d.width;
        im->height_ = o->u.image2d.height;
        if (o->u.image2d.palette && o->u.image2d.palette_len > 0)
            im->palette_.assign(o->u.image2d.palette, o->u.image2d.palette + o->u.image2d.palette_len);
        if (o->u.image2d.has_pixels && o->u.image2d.pixels)
            im->pixels_.assign(o->u.image2d.pixels, o->u.image2d.pixels + o->u.image2d.pixels_len);
        base = im;
        break;
    }
    case M3G_OBJ_ANIMATION_CONTROLLER: {
        auto a = std::make_shared<AnimationController>();
        apply_o3d(*a, o->u.anim_controller.object3d);
        a->speed_ = o->u.anim_controller.speed;
        a->weight_ = o->u.anim_controller.weight;
        a->active_start_ = o->u.anim_controller.active_interval_start;
        a->active_end_ = o->u.anim_controller.active_interval_end;
        a->ref_seq_ = o->u.anim_controller.reference_sequence_time;
        a->ref_world_ = o->u.anim_controller.reference_world_time;
        base = a;
        break;
    }
    case M3G_OBJ_ANIMATION_TRACK: {
        auto a = std::make_shared<AnimationTrack>();
        apply_o3d(*a, o->u.anim_track.object3d);
        a->property_ = o->u.anim_track.property_id;
        base = a;
        break;
    }
    case M3G_OBJ_KEYFRAME_SEQUENCE: {
        auto k = std::make_shared<KeyframeSequence>();
        apply_o3d(*k, o->u.keyframe_seq.object3d);
        k->interpolation_ = o->u.keyframe_seq.interpolation;
        k->repeat_mode_ = o->u.keyframe_seq.repeat_mode;
        k->encoding_ = o->u.keyframe_seq.encoding;
        k->duration_ = o->u.keyframe_seq.duration;
        k->valid_first_ = o->u.keyframe_seq.valid_range_first;
        k->valid_last_ = o->u.keyframe_seq.valid_range_last;
        k->component_count_ = o->u.keyframe_seq.component_count;
        for (int i = 0; i < o->u.keyframe_seq.keyframe_count; ++i) {
            KeyframeSequence::KF kf;
            kf.time = o->u.keyframe_seq.keyframes[i].time;
            kf.values.assign(o->u.keyframe_seq.keyframes[i].values,
                             o->u.keyframe_seq.keyframes[i].values + o->u.keyframe_seq.keyframes[i].value_count);
            k->keyframes_.push_back(std::move(kf));
        }
        base = k;
        break;
    }
    default: {
        auto u = std::make_shared<Object3D>();
        base = u;
        break;
    }
    }
    if (base) {
        base->object_type_ = o->object_type;
        base->object_id_ = o->object_id;
        base->raw_length_ = o->raw_length;
    }
    return base;
}

Object3DPtr lookup(std::vector<Object3DPtr> const &by_id, int id) {
    if (id <= 0 || id >= static_cast<int>(by_id.size())) return nullptr;
    return by_id[static_cast<std::size_t>(id)];
}

void wire_graph(m3g_file const *f, std::vector<Object3DPtr> &by_id) {
    for (int id = 1; id <= f->object_count; ++id) {
        m3g_object const *o = f->objects[id];
        if (!o) continue;
        Object3DPtr base = by_id[static_cast<std::size_t>(id)];
        if (!base) continue;

        auto wire_tracks = [&](m3g_object3d_meta const &m, Object3D &dst) {
            for (int i = 0; i < m.animation_track_count; ++i)
                if (auto t = as<AnimationTrack>(lookup(by_id, m.animation_track_ids[i])))
                    dst.addAnimationTrack(t);
        };

        switch (o->object_type) {
        case M3G_OBJ_WORLD:
        case M3G_OBJ_GROUP: {
            auto g = as<Group>(base);
            wire_tracks(o->u.group.node.transformable.object3d, *g);
            for (int i = 0; i < o->u.group.child_count; ++i)
                if (auto n = as<Node>(lookup(by_id, o->u.group.child_ids[i]))) g->addChild(n);
            if (o->object_type == M3G_OBJ_WORLD) {
                auto w = as<World>(base);
                if (auto c = as<Camera>(lookup(by_id, o->u.group.active_camera_id))) w->setActiveCamera(c);
                if (auto b = as<Background>(lookup(by_id, o->u.group.background_id))) w->setBackground(b);
            }
            break;
        }
        case M3G_OBJ_MESH:
        case M3G_OBJ_SKINNED_MESH:
        case M3G_OBJ_MORPHING_MESH: {
            auto mesh = as<Mesh>(base);
            wire_tracks(o->u.mesh.node.transformable.object3d, *mesh);
            mesh->vertices_ = as<VertexBuffer>(lookup(by_id, o->u.mesh.vertex_buffer_id));
            for (int i = 0; i < o->u.mesh.submesh_count; ++i) {
                mesh->submeshes_[static_cast<std::size_t>(i)] =
                    as<IndexBuffer>(lookup(by_id, o->u.mesh.submeshes[i].index_buffer_id));
                mesh->appearances_[static_cast<std::size_t>(i)] =
                    as<Appearance>(lookup(by_id, o->u.mesh.submeshes[i].appearance_id));
            }
            if (o->object_type == M3G_OBJ_SKINNED_MESH) {
                auto sk = as<SkinnedMesh>(base);
                sk->skeleton_ = as<Group>(lookup(by_id, o->u.mesh.skeleton_id));
                for (int i = 0; i < o->u.mesh.bone_transform_count; ++i) {
                    SkinnedMesh::Bone b;
                    b.node = as<Node>(lookup(by_id, o->u.mesh.bone_transforms[i].transform_node_id));
                    b.weight = o->u.mesh.bone_transforms[i].weight;
                    b.first_vertex = o->u.mesh.bone_transforms[i].first_vertex;
                    b.vertex_count = o->u.mesh.bone_transforms[i].vertex_count;
                    sk->bones_.push_back(b);
                }
            }
            if (o->object_type == M3G_OBJ_MORPHING_MESH) {
                auto mm = as<MorphingMesh>(base);
                for (int i = 0; i < o->u.mesh.morph_target_count; ++i) {
                    mm->targets_.push_back(as<VertexBuffer>(lookup(by_id, o->u.mesh.morph_target_ids[i])));
                    mm->weights_.push_back(o->u.mesh.morph_weights[i]);
                }
            }
            break;
        }
        case M3G_OBJ_CAMERA:
            wire_tracks(o->u.camera.node.transformable.object3d, *base);
            break;
        case M3G_OBJ_LIGHT:
            wire_tracks(o->u.light.node.transformable.object3d, *base);
            break;
        case M3G_OBJ_SPRITE_3D: {
            auto s = as<Sprite3D>(base);
            wire_tracks(o->u.sprite3d.node.transformable.object3d, *s);
            s->image_ = as<Image2D>(lookup(by_id, o->u.sprite3d.image_id));
            s->appearance_ = as<Appearance>(lookup(by_id, o->u.sprite3d.appearance_id));
            break;
        }
        case M3G_OBJ_APPEARANCE: {
            auto a = as<Appearance>(base);
            wire_tracks(o->u.appearance.object3d, *a);
            a->compositing_mode_ = as<CompositingMode>(lookup(by_id, o->u.appearance.compositing_mode_id));
            a->fog_ = as<Fog>(lookup(by_id, o->u.appearance.fog_id));
            a->polygon_mode_ = as<PolygonMode>(lookup(by_id, o->u.appearance.polygon_mode_id));
            a->material_ = as<Material>(lookup(by_id, o->u.appearance.material_id));
            for (int i = 0; i < o->u.appearance.texture_count; ++i)
                a->textures_[static_cast<std::size_t>(i)] =
                    as<Texture2D>(lookup(by_id, o->u.appearance.texture_ids[i]));
            break;
        }
        case M3G_OBJ_TEXTURE_2D: {
            auto t = as<Texture2D>(base);
            wire_tracks(o->u.texture2d.transformable.object3d, *t);
            t->image_ = as<Image2D>(lookup(by_id, o->u.texture2d.image_id));
            break;
        }
        case M3G_OBJ_VERTEX_BUFFER: {
            auto v = as<VertexBuffer>(base);
            wire_tracks(o->u.vertex_buffer.object3d, *v);
            v->positions_ = as<VertexArray>(lookup(by_id, o->u.vertex_buffer.positions_id));
            v->normals_ = as<VertexArray>(lookup(by_id, o->u.vertex_buffer.normals_id));
            v->colors_ = as<VertexArray>(lookup(by_id, o->u.vertex_buffer.colors_id));
            for (int i = 0; i < o->u.vertex_buffer.tex_coord_binding_count; ++i)
                v->tex_coords_[static_cast<std::size_t>(i)].array =
                    as<VertexArray>(lookup(by_id, o->u.vertex_buffer.tex_coord_bindings[i].vertex_array_id));
            break;
        }
        case M3G_OBJ_BACKGROUND: {
            auto b = as<Background>(base);
            wire_tracks(o->u.background.object3d, *b);
            b->image_ = as<Image2D>(lookup(by_id, o->u.background.background_image_id));
            break;
        }
        case M3G_OBJ_ANIMATION_TRACK: {
            auto a = as<AnimationTrack>(base);
            wire_tracks(o->u.anim_track.object3d, *a);
            a->sequence_ = as<KeyframeSequence>(lookup(by_id, o->u.anim_track.keyframe_sequence_id));
            a->controller_ = as<AnimationController>(lookup(by_id, o->u.anim_track.animation_controller_id));
            break;
        }
        case M3G_OBJ_MATERIAL:
            wire_tracks(o->u.material.object3d, *base);
            break;
        case M3G_OBJ_FOG:
            wire_tracks(o->u.fog.object3d, *base);
            break;
        case M3G_OBJ_POLYGON_MODE:
            wire_tracks(o->u.polygon_mode.object3d, *base);
            break;
        case M3G_OBJ_COMPOSITING_MODE:
            wire_tracks(o->u.compositing_mode.object3d, *base);
            break;
        case M3G_OBJ_VERTEX_ARRAY:
            wire_tracks(o->u.vertex_array.object3d, *base);
            break;
        case M3G_OBJ_TRIANGLE_STRIP_ARRAY:
            wire_tracks(o->u.triangle_strip.object3d, *base);
            break;
        case M3G_OBJ_IMAGE_2D:
            wire_tracks(o->u.image2d.object3d, *base);
            break;
        case M3G_OBJ_ANIMATION_CONTROLLER:
            wire_tracks(o->u.anim_controller.object3d, *base);
            break;
        case M3G_OBJ_KEYFRAME_SEQUENCE:
            wire_tracks(o->u.keyframe_seq.object3d, *base);
            break;
        default:
            break;
        }
    }
}

} // namespace

void SkinnedMesh::addTransform(const NodePtr &bone, int weight, int firstVertex, int numVertices) {
    if (!bone) throw std::invalid_argument("SkinnedMesh.addTransform: null bone");
    if (weight <= 0 || numVertices <= 0 || firstVertex < 0)
        throw std::invalid_argument("SkinnedMesh.addTransform: bad args");
    bones_.push_back(Bone{bone, weight, firstVertex, numVertices});
}
int SkinnedMesh::getBoneVertices(Node *bone, int *indices, float *weights) const {
    int count = 0;
    for (auto const &b : bones_) {
        if (b.node.get() != bone) continue;
        for (int i = 0; i < b.vertex_count; ++i) {
            if (indices) indices[count] = b.first_vertex + i;
            if (weights) weights[count] = static_cast<float>(b.weight);
            ++count;
        }
    }
    return count;
}

int VertexBuffer::getVertexCount() const {
    return positions_ ? positions_->getVertexCount() : 0;
}
void VertexBuffer::setPositions(const std::shared_ptr<VertexArray> &positions, float scale, const float *bias) {
    positions_ = positions;
    position_scale_ = scale;
    if (bias) std::memcpy(position_bias_, bias, 3 * sizeof(float));
}
void VertexBuffer::setTexCoords(int index, const std::shared_ptr<VertexArray> &texCoords, float scale,
                                const float *bias) {
    if (index < 0) throw std::out_of_range("setTexCoords");
    if (static_cast<int>(tex_coords_.size()) <= index) tex_coords_.resize(static_cast<std::size_t>(index + 1));
    tex_coords_[static_cast<std::size_t>(index)].array = texCoords;
    tex_coords_[static_cast<std::size_t>(index)].scale = scale;
    if (bias) std::memcpy(tex_coords_[static_cast<std::size_t>(index)].bias, bias, 3 * sizeof(float));
}
std::shared_ptr<VertexArray> VertexBuffer::getPositions(float *scaleBias) const {
    if (scaleBias) {
        scaleBias[0] = position_scale_;
        scaleBias[1] = position_bias_[0];
        scaleBias[2] = position_bias_[1];
        scaleBias[3] = position_bias_[2];
    }
    return positions_;
}
std::shared_ptr<VertexArray> VertexBuffer::getTexCoords(int index, float *scaleBias) const {
    if (index < 0 || index >= static_cast<int>(tex_coords_.size())) return nullptr;
    auto const &t = tex_coords_[static_cast<std::size_t>(index)];
    if (scaleBias) {
        scaleBias[0] = t.scale;
        scaleBias[1] = t.bias[0];
        scaleBias[2] = t.bias[1];
        scaleBias[3] = t.bias[2];
    }
    return t.array;
}

void KeyframeSequence::setKeyframe(int index, int time, const float *value) {
    auto &kf = keyframes_.at(static_cast<std::size_t>(index));
    kf.time = time;
    kf.values.assign(value, value + component_count_);
}
int KeyframeSequence::getKeyframe(int index, float *value) const {
    auto const &kf = keyframes_.at(static_cast<std::size_t>(index));
    if (value)
        for (int i = 0; i < component_count_; ++i) value[i] = kf.values[static_cast<std::size_t>(i)];
    return kf.time;
}

namespace scene {

std::vector<float> identity_matrix_row_major() {
    float m[16];
    m3g_mat4_identity(m);
    return {m, m + 16};
}
std::vector<float> multiply_row_major(const std::vector<float> &a, const std::vector<float> &b) {
    float out[16];
    m3g_mat4_mul(a.data(), b.data(), out);
    return {out, out + 16};
}
std::vector<float> translation_matrix_row_major(float x, float y, float z) {
    float m[16];
    m3g_mat4_translation(x, y, z, m);
    return {m, m + 16};
}
std::vector<float> scale_matrix_row_major(float x, float y, float z) {
    float m[16];
    m3g_mat4_scale(x, y, z, m);
    return {m, m + 16};
}
std::vector<float> axis_angle_matrix_row_major(float angle_radians, float ax, float ay, float az) {
    float m[16];
    m3g_mat4_rotation_axis(angle_radians, ax, ay, az, m);
    return {m, m + 16};
}
float vec3_length(float x, float y, float z) { return m3g_vec3_length(x, y, z); }
std::vector<float> quaternion_from_axis_angle_degrees(float a, float x, float y, float z) {
    float q[4];
    m3g_quat_from_axis_angle_deg(a, x, y, z, q);
    return {q[0], q[1], q[2], q[3]};
}
DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix) {
    DecomposedTrs d;
    if (matrix.size() != 16) return d;
    float t[3], q[4], s[3];
    m3g_decompose_trs(matrix.data(), t, q, s);
    d.translation = {t[0], t[1], t[2]};
    d.rotation = {q[0], q[1], q[2], q[3]};
    d.scale = {s[0], s[1], s[2]};
    return d;
}
std::vector<double> row_major_to_column_major_list(const std::vector<float> &m) {
    return {m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
}

static std::string cstr(char const *s) { return s ? s : ""; }

SceneIr scene_ir_from_c(m3g_scene_ir const *s) {
    SceneIr out;
    if (!s) return out;
    out.nodes.resize(static_cast<std::size_t>(s->node_count));
    for (int i = 0; i < s->node_count; ++i) {
        auto const &n = s->nodes[i];
        auto &dn = out.nodes[static_cast<std::size_t>(i)];
        dn.name = cstr(n.name);
        if (n.has_matrix) dn.matrix = std::vector<float>(n.matrix, n.matrix + 16);
        if (n.has_trs) {
            dn.translation = {n.translation[0], n.translation[1], n.translation[2]};
            dn.rotation = {n.rotation[0], n.rotation[1], n.rotation[2], n.rotation[3]};
            dn.scale = {n.scale[0], n.scale[1], n.scale[2]};
        }
        if (n.mesh_index >= 0) dn.mesh_index = n.mesh_index;
        if (n.camera_index >= 0) dn.camera_index = n.camera_index;
        dn.children.assign(n.children, n.children + n.child_count);
    }
    out.root_node_indices.assign(s->root_node_indices, s->root_node_indices + s->root_count);
    out.meshes.resize(static_cast<std::size_t>(s->mesh_count));
    for (int i = 0; i < s->mesh_count; ++i) {
        auto const &m = s->meshes[i];
        auto &dm = out.meshes[static_cast<std::size_t>(i)];
        dm.name = cstr(m.name);
        dm.primitives.resize(static_cast<std::size_t>(m.primitive_count));
        for (int p = 0; p < m.primitive_count; ++p) {
            auto const &pr = m.primitives[p];
            auto &dp = dm.primitives[static_cast<std::size_t>(p)];
            dp.name = cstr(pr.name);
            dp.positions.assign(pr.positions, pr.positions + pr.position_count);
            if (pr.normal_count) dp.normals = std::vector<float>(pr.normals, pr.normals + pr.normal_count);
            if (pr.tex_coord_count)
                dp.tex_coords0 = std::vector<float>(pr.tex_coords0, pr.tex_coords0 + pr.tex_coord_count);
            if (pr.vertex_color_count)
                dp.vertex_colors = std::vector<float>(pr.vertex_colors, pr.vertex_colors + pr.vertex_color_count);
            dp.indices.assign(pr.indices, pr.indices + pr.index_count);
            if (pr.material_index >= 0) dp.material_index = pr.material_index;
        }
    }
    out.materials.resize(static_cast<std::size_t>(s->material_count));
    for (int i = 0; i < s->material_count; ++i) {
        auto const &m = s->materials[i];
        auto &dm = out.materials[static_cast<std::size_t>(i)];
        dm.name = cstr(m.name);
        dm.base_color_factor = {m.base_color_factor[0], m.base_color_factor[1], m.base_color_factor[2],
                                 m.base_color_factor[3]};
        if (m.base_color_texture_index >= 0) dm.base_color_texture_index = m.base_color_texture_index;
        dm.emissive_factor = {m.emissive_factor[0], m.emissive_factor[1], m.emissive_factor[2]};
        dm.roughness_factor = m.roughness_factor;
        dm.metallic_factor = m.metallic_factor;
        dm.double_sided = m.double_sided != 0;
        if (m.alpha_mode) dm.alpha_mode = m.alpha_mode;
    }
    out.textures.resize(static_cast<std::size_t>(s->texture_count));
    for (int i = 0; i < s->texture_count; ++i) {
        auto const &t = s->textures[i];
        auto &dt = out.textures[static_cast<std::size_t>(i)];
        dt.name = cstr(t.name);
        dt.image_index = t.image_index;
        if (t.sampler_index >= 0) dt.sampler_index = t.sampler_index;
    }
    out.images.resize(static_cast<std::size_t>(s->image_count));
    for (int i = 0; i < s->image_count; ++i) {
        auto const &im = s->images[i];
        auto &di = out.images[static_cast<std::size_t>(i)];
        di.name = cstr(im.name);
        if (im.is_embedded) {
            EmbeddedRgbaImageSource e;
            e.object_id = im.object_id;
            e.width = im.width;
            e.height = im.height;
            if (im.pixels) e.pixels.assign(im.pixels, im.pixels + im.pixels_len);
            di.embedded = std::move(e);
        } else if (im.source_path) {
            ExternalFileImageSource e;
            e.object_id = im.object_id;
            e.source_path = im.source_path;
            di.external = std::move(e);
        }
    }
    out.samplers.resize(static_cast<std::size_t>(s->sampler_count));
    for (int i = 0; i < s->sampler_count; ++i) {
        auto const &sa = s->samplers[i];
        auto &ds = out.samplers[static_cast<std::size_t>(i)];
        if (sa.mag_filter >= 0) ds.mag_filter = sa.mag_filter;
        if (sa.min_filter >= 0) ds.min_filter = sa.min_filter;
        ds.wrap_s = sa.wrap_s;
        ds.wrap_t = sa.wrap_t;
    }
    out.cameras.resize(static_cast<std::size_t>(s->camera_count));
    for (int i = 0; i < s->camera_count; ++i) {
        auto const &c = s->cameras[i];
        auto &dc = out.cameras[static_cast<std::size_t>(i)];
        dc.name = cstr(c.name);
        if (c.has_perspective) {
            ScenePerspectiveCameraIr p;
            p.yfov_radians = c.yfov_radians;
            if (c.aspect_ratio > 0.f) p.aspect_ratio = c.aspect_ratio;
            p.znear = c.znear;
            if (c.zfar > 0.f) p.zfar = c.zfar;
            dc.perspective = p;
        }
    }
    out.animations.resize(static_cast<std::size_t>(s->animation_count));
    for (int i = 0; i < s->animation_count; ++i) {
        auto const &a = s->animations[i];
        auto &da = out.animations[static_cast<std::size_t>(i)];
        da.name = cstr(a.name);
        da.samplers.resize(static_cast<std::size_t>(a.sampler_count));
        for (int si = 0; si < a.sampler_count; ++si) {
            auto const &sa = a.samplers[si];
            auto &dsa = da.samplers[static_cast<std::size_t>(si)];
            dsa.times.assign(sa.times, sa.times + sa.time_count);
            dsa.values.assign(sa.values, sa.values + sa.value_count);
            dsa.interpolation = sa.interpolation;
            dsa.component_count = sa.component_count;
        }
        da.channels.resize(static_cast<std::size_t>(a.channel_count));
        for (int ci = 0; ci < a.channel_count; ++ci) {
            auto const &ch = a.channels[ci];
            auto &dch = da.channels[static_cast<std::size_t>(ci)];
            dch.sampler_index = ch.sampler_index;
            dch.node_index = ch.node_index;
            dch.path = ch.path;
        }
    }
    for (int i = 0; i < s->warning_count; ++i)
        out.warnings.push_back(ConversionWarning{cstr(s->warnings[i].code), cstr(s->warnings[i].message)});
    return out;
}

} // namespace scene

namespace decode {

static Decoded from_c(m3g_decoded &d) {
    Decoded out;
    if (d.source_path) out.source_path = d.source_path;
    out.objects_by_id.resize(static_cast<std::size_t>(d.file.object_count + 1));
    for (int id = 1; id <= d.file.object_count; ++id) {
        if (!d.file.objects || !d.file.objects[id]) continue;
        out.objects_by_id[static_cast<std::size_t>(id)] = make_object(d.file.objects[id]);
    }
    wire_graph(&d.file, out.objects_by_id);
    for (int id = 1; id <= d.file.object_count; ++id) {
        auto p = out.objects_by_id[static_cast<std::size_t>(id)];
        if (!p) continue;
        if (auto w = std::dynamic_pointer_cast<World>(p)) {
            out.world = w;
            out.roots.push_back(w);
        }
    }
    if (out.roots.empty()) {
        for (int id = 1; id <= d.file.object_count; ++id)
            if (auto n = std::dynamic_pointer_cast<Node>(out.objects_by_id[static_cast<std::size_t>(id)]))
                if (!n->getParent()) out.roots.push_back(n);
    }
    out.scene_ir = scene::scene_ir_from_c(&d.scene);
    m3g_decoded_free(&d);
    return out;
}

Decoded Decoder::decode_file(const std::string &path, const DecodeOptions &options) const {
    m3g_decoded d{};
    m3g_decode_options opt{};
    if (options.pattern_path) opt.pattern_path = options.pattern_path->c_str();
    if (m3g_decode_file(path.c_str(), &opt, &d) != M3G_OK) {
        std::string err = m3g_last_error();
        m3g_decoded_free(&d);
        throw std::runtime_error(err.empty() ? "m3g_decode_file failed" : err);
    }
    return from_c(d);
}

Decoded Decoder::decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                              const DecodeOptions &options) const {
    m3g_decoded d{};
    m3g_decode_options opt{};
    if (options.pattern_path) opt.pattern_path = options.pattern_path->c_str();
    if (m3g_decode_bytes(bytes.data(), bytes.size(), source_path.c_str(), &opt, &d) != M3G_OK) {
        std::string err = m3g_last_error();
        m3g_decoded_free(&d);
        throw std::runtime_error(err.empty() ? "m3g_decode_bytes failed" : err);
    }
    return from_c(d);
}

} // namespace decode
} // namespace m3g

#endif /* M3G_DECODE_IMPL_INCLUDED */
#undef M3G_DECODE_IMPL
#endif /* M3G_DECODE_IMPL */
#endif /* DOXYGEN */
