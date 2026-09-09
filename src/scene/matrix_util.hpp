#pragma once

#include "slop/m3g_model.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

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
