// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include "cgl/common/vector.h"

// -----------------------------------------------------------------------------
TEST(common_vector_test, ConstructorAndAccess) {
    // constructor test
    cgl::Vec3f v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v[0], 1.0f);
    EXPECT_FLOAT_EQ(v[1], 2.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);

    // zero vector test
    cgl::Vec4i v_zero;
    EXPECT_EQ(v_zero[0], 0);
    EXPECT_EQ(v_zero[3], 0);

    // Testing constructors with parameters, missing items are zero-initialized.
    cgl::Vector<4, float> v_partial(5.0f, 6.0f);
    EXPECT_FLOAT_EQ(v_partial[0], 5.0f);
    EXPECT_FLOAT_EQ(v_partial[1], 6.0f);
    EXPECT_FLOAT_EQ(v_partial[2], 0.0f);
}

// -----------------------------------------------------------------------------
TEST(common_vector_test, AdditionAndSubtraction) {
    cgl::Vec3f a(1.0f, 2.0f, 3.0f);
    cgl::Vec3f b(5.0f, 1.0f, -2.0f);

    // test add
    cgl::Vec3f sum = a + b;
    EXPECT_FLOAT_EQ(sum[0], 6.0f);
    EXPECT_FLOAT_EQ(sum[1], 3.0f);
    EXPECT_FLOAT_EQ(sum[2], 1.0f);

    // test sub
    cgl::Vec3f diff = a - b;
    EXPECT_FLOAT_EQ(diff[0], -4.0f);
    EXPECT_FLOAT_EQ(diff[1], 1.0f);
    EXPECT_FLOAT_EQ(diff[2], 5.0f);
}

// -----------------------------------------------------------------------------
TEST(common_vector_test, ScalarMultiplicationAndDivision) {
    cgl::Vec3f v(2.0f, 4.0f, 8.0f);
    float scalar = 2.0f;

    // test scalar multiplication
    cgl::Vec3f multiplied = v * scalar;
    EXPECT_FLOAT_EQ(multiplied[0], 4.0f);
    EXPECT_FLOAT_EQ(multiplied[1], 8.0f);
    EXPECT_FLOAT_EQ(multiplied[2], 16.0f);

    // test scalar division
    cgl::Vec3f divided = v / scalar;
    EXPECT_FLOAT_EQ(divided[0], 1.0f);
    EXPECT_FLOAT_EQ(divided[1], 2.0f);
    EXPECT_FLOAT_EQ(divided[2], 4.0f);

    // test int type
    cgl::Vec2i i_v(10, 20);
    cgl::Vec2i i_mult = i_v * 5;
    EXPECT_EQ(i_mult[0], 50);
    EXPECT_EQ(i_mult[1], 100);
}

// -----------------------------------------------------------------------------
TEST(common_vector_test, DotProduct) {
    cgl::Vec3f a(1.0f, 2.0f, 3.0f);
    cgl::Vec3f b(4.0f, 5.0f, 6.0f);
    // excepted: (1*4) + (2*5) + (3*6) = 4 + 10 + 18 = 32.0

    float result = dot(a, b);
    EXPECT_FLOAT_EQ(result, 32.0f);

    // test Orthogonal vectors (dot = 0)
    cgl::Vec2f c(1.0f, 0.0f);
    cgl::Vec2f d(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(dot(c, d), 0.0f);

    // neg value test
    cgl::Vec3f e(-1.0f, 0.0f, -2.0f);
    cgl::Vec3f f(1.0f, 0.0f, 2.0f);
    // (-1*1) + (0*0) + (-2*2) = -1 + 0 - 4 = -5.0
    EXPECT_FLOAT_EQ(dot(e, f), -5.0f);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
