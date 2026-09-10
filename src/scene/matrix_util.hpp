#pragma once

#include "slop/m3g_model.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace slop {
namespace scene {

inline std::vector<float> identity_matrix_row_major() {
    return {
        1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> multiply_row_major(const std::vector<float> &left, const std::vector<float> &right) {
    if (left.size() != 16 || right.size() != 16) {
        throw std::invalid_argument("Matrix multiplication expects 4x4 matrices");
    }
    std::vector<float> result(16);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.f;
            for (int index = 0; index < 4; ++index) {
                sum += left[static_cast<std::size_t>(row * 4 + index)] *
                       right[static_cast<std::size_t>(index * 4 + col)];
            }
            result[static_cast<std::size_t>(row * 4 + col)] = sum;
        }
    }
    return result;
}

inline std::vector<float> translation_matrix_row_major(float x, float y, float z) {
    auto m = identity_matrix_row_major();
    m[3] = x;
    m[7] = y;
    m[11] = z;
    return m;
}

inline std::vector<float> scale_matrix_row_major(float x, float y, float z) {
    return {
        x, 0.f, 0.f, 0.f, 0.f, y, 0.f, 0.f, 0.f, 0.f, z, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> axis_angle_matrix_row_major(float angle_radians, float axis_x, float axis_y, float axis_z) {
    const float length = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
    if (length < 1e-6f || angle_radians == 0.f) {
        return identity_matrix_row_major();
    }
    const float x = axis_x / length;
    const float y = axis_y / length;
    const float z = axis_z / length;
    const float c = std::cos(angle_radians);
    const float s = std::sin(angle_radians);
    const float t = 1.f - c;
    return {
        t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.f, t * x * y + s * z, t * y * y + c,
        t * y * z - s * x, 0.f, t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> component_transform_to_row_major(const m3g::ComponentTransform &component) {
    const auto translation =
        translation_matrix_row_major(component.translation[0], component.translation[1], component.translation[2]);
    const auto rotation = axis_angle_matrix_row_major(component.orientation_angle, component.orientation_axis[0],
                                                      component.orientation_axis[1], component.orientation_axis[2]);
    const auto scale = scale_matrix_row_major(component.scale[0], component.scale[1], component.scale[2]);
    return multiply_row_major(translation, multiply_row_major(rotation, scale));
}

inline std::vector<float> node_matrix_row_major(const m3g::NodeMeta &node_meta) {
    if (node_meta.transformable.general_transform) {
        return *node_meta.transformable.general_transform;
    }
    if (node_meta.transformable.component_transform) {
        return component_transform_to_row_major(*node_meta.transformable.component_transform);
    }
    return identity_matrix_row_major();
}

struct DecomposedTrs {
    std::vector<float> translation{0.f, 0.f, 0.f};
    std::vector<float> rotation{0.f, 0.f, 0.f, 1.f}; // xyzw
    std::vector<float> scale{1.f, 1.f, 1.f};
};

inline float vec3_length(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }

inline std::vector<float> quaternion_from_axis_angle_degrees(float angle_degrees, float ax, float ay, float az) {
    const float len = vec3_length(ax, ay, az);
    if (len < 1e-8f || std::abs(angle_degrees) < 1e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    const float half = (angle_degrees * static_cast<float>(M_PI) / 180.f) * 0.5f;
    const float s = std::sin(half);
    const float c = std::cos(half);
    return {ax / len * s, ay / len * s, az / len * s, c};
}

inline std::vector<float> quaternion_from_row_major_rotation(const float r[3][3]) {
    const float trace = r[0][0] + r[1][1] + r[2][2];
    float x, y, z, w;
    if (trace > 0.f) {
        const float s = std::sqrt(trace + 1.f) * 2.f;
        w = 0.25f * s;
        x = (r[2][1] - r[1][2]) / s;
        y = (r[0][2] - r[2][0]) / s;
        z = (r[1][0] - r[0][1]) / s;
    } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
        const float s = std::sqrt(1.f + r[0][0] - r[1][1] - r[2][2]) * 2.f;
        w = (r[2][1] - r[1][2]) / s;
        x = 0.25f * s;
        y = (r[0][1] + r[1][0]) / s;
        z = (r[0][2] + r[2][0]) / s;
    } else if (r[1][1] > r[2][2]) {
        const float s = std::sqrt(1.f + r[1][1] - r[0][0] - r[2][2]) * 2.f;
        w = (r[0][2] - r[2][0]) / s;
        x = (r[0][1] + r[1][0]) / s;
        y = 0.25f * s;
        z = (r[1][2] + r[2][1]) / s;
    } else {
        const float s = std::sqrt(1.f + r[2][2] - r[0][0] - r[1][1]) * 2.f;
        w = (r[1][0] - r[0][1]) / s;
        x = (r[0][2] + r[2][0]) / s;
        y = (r[1][2] + r[2][1]) / s;
        z = 0.25f * s;
    }
    const float qlen = std::sqrt(x * x + y * y + z * z + w * w);
    if (qlen < 1e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    return {x / qlen, y / qlen, z / qlen, w / qlen};
}

inline DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix) {
    DecomposedTrs out;
    if (matrix.size() != 16) {
        return out;
    }
    out.translation = {matrix[3], matrix[7], matrix[11]};
    float sx = vec3_length(matrix[0], matrix[4], matrix[8]);
    float sy = vec3_length(matrix[1], matrix[5], matrix[9]);
    float sz = vec3_length(matrix[2], matrix[6], matrix[10]);
    const float det = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) -
                      matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) +
                      matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
    if (det < 0.f) {
        sx = -sx;
    }
    if (sx == 0.f) {
        sx = 1e-8f;
    }
    if (sy == 0.f) {
        sy = 1e-8f;
    }
    if (sz == 0.f) {
        sz = 1e-8f;
    }
    out.scale = {sx, sy, sz};
    float r[3][3] = {
        {matrix[0] / sx, matrix[1] / sy, matrix[2] / sz},
        {matrix[4] / sx, matrix[5] / sy, matrix[6] / sz},
        {matrix[8] / sx, matrix[9] / sy, matrix[10] / sz},
    };
    out.rotation = quaternion_from_row_major_rotation(r);
    return out;
}

inline std::vector<double> row_major_to_column_major_list(const std::vector<float> &matrix) {
    if (matrix.size() != 16) {
        throw std::invalid_argument("Expected a 4x4 matrix");
    }
    return {
        matrix[0],  matrix[4],  matrix[8],  matrix[12], matrix[1],  matrix[5],  matrix[9],  matrix[13],
        matrix[2],  matrix[6],  matrix[10], matrix[14], matrix[3],  matrix[7],  matrix[11], matrix[15],
    };
}

} // namespace scene
} // namespace slop
