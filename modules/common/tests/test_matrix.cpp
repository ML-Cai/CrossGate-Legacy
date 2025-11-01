// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include "cgl/common/matrix.h"

// -----------------------------------------------------------------------------
TEST(common_math_test, OrthoSymmetryCheck) {
    float l = -10.0f;
    float r = 10.0f;
    float b = -5.0f;
    float t = 5.0f;
    float n = 1.0f;
    float f = 100.0f;

    // excepted value
    const float expected_m00 = 0.1f;    // 2 / 20
    const float expected_m11 = -0.2f;   // -2 / 10 (Y-Flip)
    const float expected_m22 = 1.0f / 99.0f;
    const float expected_m03 = 0.0f;    // X trans should be 0
    const float expected_m13 = 0.0f;    // Y trans should be 0
    const float expected_m23 = -1.0f / 99.0f;

    // Tolerance
    const float epsilon = 1e-4f;

    cgl::Mat4x4 m = cgl::ortho(l, r, b, t, n, f);

    // 1. check scale
    EXPECT_NEAR(m(0, 0), expected_m00, epsilon) << "X Scale (M[0,0]) check failed.";
    EXPECT_NEAR(m(1, 1), expected_m11, epsilon) << "Y Scale (M[1,1]) check failed (Y-Flip).";
    EXPECT_NEAR(m(2, 2), expected_m22, epsilon) << "Z Scale (M[2,2]) check failed (0-1 range).";

    // 2. check trans
    EXPECT_NEAR(m(0, 3), expected_m03, epsilon) << "X Translation (M[0,3]) check failed.";
    EXPECT_NEAR(m(1, 3), expected_m13, epsilon) << "Y Translation (M[1,3]) check failed.";
    EXPECT_NEAR(m(2, 3), expected_m23, epsilon) << "Z Translation (M[2,3]) check failed.";

    // 3. check const value
    EXPECT_FLOAT_EQ(m(3, 3), 1.0f) << "M[3,3] must be 1.0.";

    // 4. check others
    EXPECT_FLOAT_EQ(m(0, 1), 0.0f) << "M[0,1] must be 0.0.";
    EXPECT_FLOAT_EQ(m(2, 1), 0.0f) << "M[2,1] must be 0.0.";
}

// -----------------------------------------------------------------------------
TEST(common_math_test, OrthoAsymmetryAndEdges) {
    float l = 1.0f;
    float r = 5.0f;
    float b = 10.0f;
    float t = 20.0f;
    float n = 0.0f; // near plane at 0
    float f = 50.0f;

    // Excepted value
    // X Scale: 2 / (5 - 1) = 2 / 4 = 0.5
    // Y Scale: -2 / (20 - 10) = -2 / 10 = -0.2
    // Z Scale: 1 / (50 - 0) = 0.02
    // X Trans: -(5 + 1) / 4 = -6 / 4 = -1.5
    // Y Trans: -(20 + 10) / 10 = -30 / 10 = -3.0
    // Z Trans: -0 / 50 = 0.0

    cgl::Mat4x4 m = cgl::ortho(l, r, b, t, n, f);
    const float epsilon = 1e-4f;

    // scale
    EXPECT_NEAR(m(0, 0), 0.5f, epsilon);
    EXPECT_NEAR(m(1, 1), -0.2f, epsilon);
    EXPECT_NEAR(m(2, 2), 0.02f, epsilon);

    // Trans
    EXPECT_NEAR(m(0, 3), -1.5f, epsilon);
    EXPECT_NEAR(m(1, 3), -3.0f, epsilon);
    EXPECT_NEAR(m(2, 3), 0.0f, epsilon); // sepcial case：near=0
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
