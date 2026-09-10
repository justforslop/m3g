/* Unit tests: m3g::scene matrix helpers. */
#include "testfw.h"

#include <m3g.hpp>

#include <cmath>
#include <vector>

static bool near_f(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

static bool vec_near(const std::vector<float> &a, const std::vector<float> &b, float eps = 1e-4f)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!near_f(a[i], b[i], eps)) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    TESTFW_INIT();

    using namespace m3g::scene;

    TESTFW_TEST_BEGIN("identity_matrix_row_major");
    {
        auto I = identity_matrix_row_major();
        TESTFW_EXPECTED(I.size() == 16u);
        TESTFW_EXPECTED(m3g::model::is_identity_row_major(I));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("translation_matrix_row_major");
    {
        auto T = translation_matrix_row_major(1.f, 2.f, 3.f);
        TESTFW_EXPECTED(near_f(T[3], 1.f));
        TESTFW_EXPECTED(near_f(T[7], 2.f));
        TESTFW_EXPECTED(near_f(T[11], 3.f));
        TESTFW_EXPECTED(near_f(T[0], 1.f));
        TESTFW_EXPECTED(near_f(T[15], 1.f));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("scale_matrix_row_major");
    {
        auto S = scale_matrix_row_major(2.f, 3.f, 4.f);
        TESTFW_EXPECTED(near_f(S[0], 2.f));
        TESTFW_EXPECTED(near_f(S[5], 3.f));
        TESTFW_EXPECTED(near_f(S[10], 4.f));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("multiply_row_major: I * T == T");
    {
        auto I = identity_matrix_row_major();
        auto T = translation_matrix_row_major(5.f, 0.f, -1.f);
        auto R = multiply_row_major(I, T);
        TESTFW_EXPECTED(vec_near(R, T));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("row_major_to_column_major_list");
    {
        std::vector<float> row = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        };
        auto col = row_major_to_column_major_list(row);
        TESTFW_EXPECTED(col.size() == 16u);
        TESTFW_EXPECTED(near_f(static_cast<float>(col[0]), 1.f));
        TESTFW_EXPECTED(near_f(static_cast<float>(col[1]), 5.f));
        TESTFW_EXPECTED(near_f(static_cast<float>(col[4]), 2.f));
        TESTFW_EXPECTED(near_f(static_cast<float>(col[15]), 16.f));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("decompose_row_major_trs: pure translation");
    {
        auto T = translation_matrix_row_major(10.f, -2.f, 0.5f);
        DecomposedTrs d = decompose_row_major_trs(T);
        TESTFW_EXPECTED(near_f(d.translation[0], 10.f));
        TESTFW_EXPECTED(near_f(d.translation[1], -2.f));
        TESTFW_EXPECTED(near_f(d.translation[2], 0.5f));
        TESTFW_EXPECTED(near_f(d.scale[0], 1.f));
        TESTFW_EXPECTED(near_f(d.scale[1], 1.f));
        TESTFW_EXPECTED(near_f(d.scale[2], 1.f));
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("quaternion_from_axis_angle_degrees: zero angle is identity quat");
    {
        auto q = quaternion_from_axis_angle_degrees(0.f, 0.f, 1.f, 0.f);
        TESTFW_EXPECTED(q.size() == 4u);
        TESTFW_EXPECTED(near_f(q[0], 0.f));
        TESTFW_EXPECTED(near_f(q[1], 0.f));
        TESTFW_EXPECTED(near_f(q[2], 0.f));
        TESTFW_EXPECTED(near_f(q[3], 1.f));
    }
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
