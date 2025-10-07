/**
 * @file test_math.cpp
 * @brief Unit tests for math utilities
 * 
 * Tests vector operations, matrix operations, quaternions, and math helpers.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/core/math.hpp>

#include <gtest/gtest.h>

using namespace luma;

/**
 * @brief Test fixture for math tests
 */
class MathTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    // helper for float comparison with tolerance
    void expect_near(float a, float b, float tolerance = 1e-5f) {
        EXPECT_NEAR(a, b, tolerance);
    }
    
    void expect_vec3_near(const vec3& a, const vec3& b, float tolerance = 1e-5f) {
        EXPECT_NEAR(a.x, b.x, tolerance);
        EXPECT_NEAR(a.y, b.y, tolerance);
        EXPECT_NEAR(a.z, b.z, tolerance);
    }
};

// ==============================================================================
// Vector Operations
// ==============================================================================

/**
 * @brief Test vector construction
 */
TEST_F(MathTest, VectorConstruction) {
    vec2 v2{1.0f, 2.0f};
    EXPECT_FLOAT_EQ(v2.x, 1.0f);
    EXPECT_FLOAT_EQ(v2.y, 2.0f);
    
    vec3 v3{1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(v3.x, 1.0f);
    EXPECT_FLOAT_EQ(v3.y, 2.0f);
    EXPECT_FLOAT_EQ(v3.z, 3.0f);
    
    vec4 v4{1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_FLOAT_EQ(v4.x, 1.0f);
    EXPECT_FLOAT_EQ(v4.y, 2.0f);
    EXPECT_FLOAT_EQ(v4.z, 3.0f);
    EXPECT_FLOAT_EQ(v4.w, 4.0f);
}

/**
 * @brief Test vector addition
 */
TEST_F(MathTest, VectorAddition) {
    vec3 a{1.0f, 2.0f, 3.0f};
    vec3 b{4.0f, 5.0f, 6.0f};
    vec3 c = a + b;
    
    expect_vec3_near(c, vec3{5.0f, 7.0f, 9.0f});
}

/**
 * @brief Test vector subtraction
 */
TEST_F(MathTest, VectorSubtraction) {
    vec3 a{5.0f, 7.0f, 9.0f};
    vec3 b{1.0f, 2.0f, 3.0f};
    vec3 c = a - b;
    
    expect_vec3_near(c, vec3{4.0f, 5.0f, 6.0f});
}

/**
 * @brief Test vector scalar multiplication
 */
TEST_F(MathTest, VectorScalarMultiplication) {
    vec3 v{1.0f, 2.0f, 3.0f};
    vec3 scaled = v * 2.0f;
    
    expect_vec3_near(scaled, vec3{2.0f, 4.0f, 6.0f});
}

/**
 * @brief Test dot product
 */
TEST_F(MathTest, DotProduct) {
    vec3 a{1.0f, 0.0f, 0.0f};
    vec3 b{0.0f, 1.0f, 0.0f};
    vec3 c{1.0f, 0.0f, 0.0f};
    
    // perpendicular vectors have dot product 0
    expect_near(glm::dot(a, b), 0.0f);
    
    // parallel vectors have dot product 1 (if normalized)
    expect_near(glm::dot(a, c), 1.0f);
}

/**
 * @brief Test cross product
 */
TEST_F(MathTest, CrossProduct) {
    vec3 x_axis{1.0f, 0.0f, 0.0f};
    vec3 y_axis{0.0f, 1.0f, 0.0f};
    
    // x cross y = z
    vec3 z = glm::cross(x_axis, y_axis);
    expect_vec3_near(z, vec3{0.0f, 0.0f, 1.0f});
    
    // y cross x = -z
    vec3 neg_z = glm::cross(y_axis, x_axis);
    expect_vec3_near(neg_z, vec3{0.0f, 0.0f, -1.0f});
}

/**
 * @brief Test vector length
 */
TEST_F(MathTest, VectorLength) {
    vec3 v{3.0f, 4.0f, 0.0f};
    
    // 3-4-5 triangle
    expect_near(glm::length(v), 5.0f);
}

/**
 * @brief Test vector normalization
 */
TEST_F(MathTest, VectorNormalization) {
    vec3 v{3.0f, 4.0f, 0.0f};
    vec3 normalized = glm::normalize(v);
    
    // length should be 1
    expect_near(glm::length(normalized), 1.0f);
    
    // direction should be preserved (scaled)
    expect_vec3_near(normalized, vec3{0.6f, 0.8f, 0.0f});
}

// ==============================================================================
// Matrix Operations
// ==============================================================================

/**
 * @brief Test matrix construction
 */
TEST_F(MathTest, MatrixConstruction) {
    // identity matrix
    mat4 identity = mat4(1.0f);
    
    EXPECT_FLOAT_EQ(identity[0][0], 1.0f);
    EXPECT_FLOAT_EQ(identity[1][1], 1.0f);
    EXPECT_FLOAT_EQ(identity[2][2], 1.0f);
    EXPECT_FLOAT_EQ(identity[3][3], 1.0f);
    
    EXPECT_FLOAT_EQ(identity[0][1], 0.0f);
    EXPECT_FLOAT_EQ(identity[1][0], 0.0f);
}

/**
 * @brief Test matrix multiplication
 */
TEST_F(MathTest, MatrixMultiplication) {
    mat4 identity = mat4(1.0f);
    mat4 scale = glm::scale(mat4(1.0f), vec3{2.0f, 2.0f, 2.0f});
    
    // identity * scale = scale
    mat4 result = identity * scale;
    
    EXPECT_FLOAT_EQ(result[0][0], 2.0f);
    EXPECT_FLOAT_EQ(result[1][1], 2.0f);
    EXPECT_FLOAT_EQ(result[2][2], 2.0f);
}

/**
 * @brief Test matrix-vector multiplication
 */
TEST_F(MathTest, MatrixVectorMultiplication) {
    mat4 scale = glm::scale(mat4(1.0f), vec3{2.0f, 3.0f, 4.0f});
    vec4 v{1.0f, 1.0f, 1.0f, 1.0f};
    
    vec4 result = scale * v;
    
    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 3.0f);
    EXPECT_FLOAT_EQ(result.z, 4.0f);
    EXPECT_FLOAT_EQ(result.w, 1.0f);
}

/**
 * @brief Test translation matrix
 */
TEST_F(MathTest, TranslationMatrix) {
    vec3 translation{1.0f, 2.0f, 3.0f};
    mat4 translate = glm::translate(mat4(1.0f), translation);
    
    vec4 point{0.0f, 0.0f, 0.0f, 1.0f};
    vec4 translated = translate * point;
    
    EXPECT_FLOAT_EQ(translated.x, 1.0f);
    EXPECT_FLOAT_EQ(translated.y, 2.0f);
    EXPECT_FLOAT_EQ(translated.z, 3.0f);
    EXPECT_FLOAT_EQ(translated.w, 1.0f);
}

/**
 * @brief Test rotation matrix
 */
TEST_F(MathTest, RotationMatrix) {
    // rotate 90 degrees around Z axis
    mat4 rotate = glm::rotate(mat4(1.0f), glm::radians(90.0f), vec3{0.0f, 0.0f, 1.0f});
    
    vec4 x_axis{1.0f, 0.0f, 0.0f, 1.0f};
    vec4 rotated = rotate * x_axis;
    
    // should become Y axis
    expect_near(rotated.x, 0.0f, 1e-5f);
    expect_near(rotated.y, 1.0f, 1e-5f);
    expect_near(rotated.z, 0.0f, 1e-5f);
}

/**
 * @brief Test perspective projection matrix
 */
TEST_F(MathTest, PerspectiveMatrix) {
    float fov = glm::radians(45.0f);
    float aspect = 16.0f / 9.0f;
    float near = 0.1f;
    float far = 100.0f;
    
    mat4 proj = glm::perspective(fov, aspect, near, far);
    
    // should not be identity
    EXPECT_NE(proj[0][0], 1.0f);
    
    // test passes if no crash
    SUCCEED();
}

// ==============================================================================
// Quaternion Operations
// ==============================================================================

/**
 * @brief Test quaternion construction
 */
TEST_F(MathTest, QuaternionConstruction) {
    // identity quaternion (no rotation)
    quat identity = quat(1.0f, 0.0f, 0.0f, 0.0f);
    
    EXPECT_FLOAT_EQ(identity.w, 1.0f);
    EXPECT_FLOAT_EQ(identity.x, 0.0f);
    EXPECT_FLOAT_EQ(identity.y, 0.0f);
    EXPECT_FLOAT_EQ(identity.z, 0.0f);
}

/**
 * @brief Test quaternion from axis-angle
 */
TEST_F(MathTest, QuaternionFromAxisAngle) {
    // 90 degree rotation around Z axis
    quat q = glm::angleAxis(glm::radians(90.0f), vec3{0.0f, 0.0f, 1.0f});
    
    vec3 x_axis{1.0f, 0.0f, 0.0f};
    vec3 rotated = q * x_axis;
    
    // should become Y axis
    expect_near(rotated.x, 0.0f, 1e-5f);
    expect_near(rotated.y, 1.0f, 1e-5f);
    expect_near(rotated.z, 0.0f, 1e-5f);
}

/**
 * @brief Test quaternion multiplication
 */
TEST_F(MathTest, QuaternionMultiplication) {
    // two 90-degree rotations = 180-degree rotation
    quat q1 = glm::angleAxis(glm::radians(90.0f), vec3{0.0f, 0.0f, 1.0f});
    quat q2 = glm::angleAxis(glm::radians(90.0f), vec3{0.0f, 0.0f, 1.0f});
    quat combined = q1 * q2;
    
    vec3 x_axis{1.0f, 0.0f, 0.0f};
    vec3 rotated = combined * x_axis;
    
    // should become -X axis
    expect_near(rotated.x, -1.0f, 1e-5f);
    expect_near(rotated.y, 0.0f, 1e-5f);
    expect_near(rotated.z, 0.0f, 1e-5f);
}

// ==============================================================================
// Math Helper Functions
// ==============================================================================

/**
 * @brief Test clamp function
 */
TEST_F(MathTest, ClampFunction) {
    EXPECT_FLOAT_EQ(clamp(5.0f, 0.0f, 10.0f), 5.0f);  // in range
    EXPECT_FLOAT_EQ(clamp(-5.0f, 0.0f, 10.0f), 0.0f); // below min
    EXPECT_FLOAT_EQ(clamp(15.0f, 0.0f, 10.0f), 10.0f); // above max
}

/**
 * @brief Test lerp function
 */
TEST_F(MathTest, LerpFunction) {
    expect_near(lerp(0.0f, 10.0f, 0.0f), 0.0f);   // t=0
    expect_near(lerp(0.0f, 10.0f, 1.0f), 10.0f);  // t=1
    expect_near(lerp(0.0f, 10.0f, 0.5f), 5.0f);   // t=0.5
}

/**
 * @brief Test smoothstep function
 */
TEST_F(MathTest, SmoothstepFunction) {
    // smoothstep should be 0 at edges, smooth curve in middle
    expect_near(smoothstep(0.0f, 1.0f, 0.0f), 0.0f);
    expect_near(smoothstep(0.0f, 1.0f, 1.0f), 1.0f);
    
    float mid = smoothstep(0.0f, 1.0f, 0.5f);
    EXPECT_GT(mid, 0.0f);
    EXPECT_LT(mid, 1.0f);
    EXPECT_NEAR(mid, 0.5f, 0.1f); // should be around 0.5
}

/**
 * @brief Test radians/degrees conversion
 */
TEST_F(MathTest, AngleConversion) {
    expect_near(radians(0.0f), 0.0f);
    expect_near(radians(180.0f), constants::pi);
    expect_near(radians(360.0f), constants::two_pi);
    
    expect_near(degrees(0.0f), 0.0f);
    expect_near(degrees(constants::pi), 180.0f);
    expect_near(degrees(constants::two_pi), 360.0f);
}

/**
 * @brief Test approx_equal function
 */
TEST_F(MathTest, ApproxEqual) {
    EXPECT_TRUE(approx_equal(1.0f, 1.0f));
    EXPECT_TRUE(approx_equal(1.0f, 1.0f + 1e-7f)); // within epsilon
    EXPECT_FALSE(approx_equal(1.0f, 2.0f)); // clearly different
}

/**
 * @brief Test approx_zero function
 */
TEST_F(MathTest, ApproxZero) {
    EXPECT_TRUE(approx_zero(0.0f));
    EXPECT_TRUE(approx_zero(1e-7f)); // within epsilon
    EXPECT_FALSE(approx_zero(1.0f)); // clearly not zero
}

/**
 * @brief Test sign function
 */
TEST_F(MathTest, SignFunction) {
    EXPECT_EQ(sign(5.0f), 1.0f);
    EXPECT_EQ(sign(-5.0f), -1.0f);
    EXPECT_EQ(sign(0.0f), 0.0f);
}

/**
 * @brief Test min/max functions
 */
TEST_F(MathTest, MinMaxFunctions) {
    EXPECT_EQ(min(5, 10), 5);
    EXPECT_EQ(min(10, 5), 5);
    
    EXPECT_EQ(max(5, 10), 10);
    EXPECT_EQ(max(10, 5), 10);
}
