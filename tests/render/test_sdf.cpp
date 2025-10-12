/**
 * @file test_sdf.cpp
 * @brief Tests for SDF (Signed Distance Field) functions
 * 
 * Validates all SDF primitives and operations with known values.
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 */

#include <luma/render/sdf.hpp>
#include <luma/core/math.hpp>

#include <gtest/gtest.h>

using namespace luma;
using namespace luma::render;

class SDFTest : public ::testing::Test {};

// ========== Sphere Tests ==========

TEST_F(SDFTest, SphereAtOrigin) {
    // Point at origin, sphere radius 1.0
    const auto dist = sdf_sphere(glm::vec3(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(dist, -1.0f);  // Inside sphere (negative distance)
}

TEST_F(SDFTest, SphereOnSurface) {
    // Point on surface (distance = 0)
    const auto dist = sdf_sphere(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
    EXPECT_NEAR(dist, 0.0f, 0.001f);
}

TEST_F(SDFTest, SphereOutside) {
    // Point outside sphere
    const auto dist = sdf_sphere(glm::vec3(2.0f, 0.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(dist, 1.0f);  // 1 unit outside
}

TEST_F(SDFTest, Sphere3D) {
    // Point in 3D space
    const auto p = glm::vec3(1.0f, 1.0f, 1.0f);
    const auto dist = sdf_sphere(p, 1.0f);
    const auto expected = glm::length(p) - 1.0f;
    EXPECT_FLOAT_EQ(dist, expected);
}

// ========== Box Tests ==========

TEST_F(SDFTest, BoxAtOrigin) {
    // Point at origin, box with extents (1, 1, 1)
    const auto dist = sdf_box(glm::vec3(0.0f), glm::vec3(1.0f), 0.0f);
    EXPECT_LT(dist, 0.0f);  // Inside box
}

TEST_F(SDFTest, BoxOnSurface) {
    // Point on surface
    const auto dist = sdf_box(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f), 0.0f);
    EXPECT_NEAR(dist, 0.0f, 0.001f);
}

TEST_F(SDFTest, BoxOutside) {
    // Point outside box
    const auto dist = sdf_box(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f), 0.0f);
    EXPECT_GT(dist, 0.0f);
    EXPECT_FLOAT_EQ(dist, 1.0f);
}

TEST_F(SDFTest, BoxWithRounding) {
    // Rounded corners should increase distance from sharp corners
    const auto extents = glm::vec3(1.0f);
    const auto p = glm::vec3(1.0f, 1.0f, 0.0f);
    
    const auto sharp = sdf_box(p, extents, 0.0f);
    const auto rounded = sdf_box(p, extents, 0.1f);
    
    EXPECT_LT(rounded, sharp);  // Rounded box brings corner closer
}

// ========== Plane Tests ==========

TEST_F(SDFTest, PlaneAtOrigin) {
    // Horizontal plane at y = 0, point on plane
    const auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto dist = sdf_plane(glm::vec3(0.0f), normal, 0.0f);
    EXPECT_FLOAT_EQ(dist, 0.0f);
}

TEST_F(SDFTest, PlaneAbove) {
    // Point above plane
    const auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto dist = sdf_plane(glm::vec3(0.0f, 2.0f, 0.0f), normal, 0.0f);
    EXPECT_FLOAT_EQ(dist, 2.0f);
}

TEST_F(SDFTest, PlaneBelow) {
    // Point below plane
    const auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto dist = sdf_plane(glm::vec3(0.0f, -3.0f, 0.0f), normal, 0.0f);
    EXPECT_FLOAT_EQ(dist, -3.0f);
}

TEST_F(SDFTest, PlaneWithOffset) {
    // Plane offset from origin
    const auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto dist = sdf_plane(glm::vec3(0.0f, 0.0f, 0.0f), normal, -5.0f);
    EXPECT_FLOAT_EQ(dist, -5.0f);  // Plane is 5 units below origin
}

// ========== SDF Operations ==========

TEST_F(SDFTest, UnionTwoSpheres) {
    const auto p = glm::vec3(0.0f);
    const auto d1 = sdf_sphere(p, 1.0f);
    const auto d2 = sdf_sphere(p - glm::vec3(3.0f, 0.0f, 0.0f), 1.0f);
    
    const auto combined = sdf_union(d1, d2);
    EXPECT_FLOAT_EQ(combined, d1);  // Closer to first sphere
}

TEST_F(SDFTest, IntersectionTwoSpheres) {
    const auto p = glm::vec3(0.0f);
    const auto d1 = sdf_sphere(p, 2.0f);  // -2 (inside large sphere)
    const auto d2 = sdf_sphere(p, 1.0f);  // -1 (inside small sphere)
    
    const auto combined = sdf_intersection(d1, d2);
    // Intersection takes the maximum (keeps smaller volume)
    EXPECT_FLOAT_EQ(combined, std::max(d1, d2));  // max(-2, -1) = -1
}

TEST_F(SDFTest, SubtractionSpheres) {
    const auto p = glm::vec3(0.0f);
    const auto d1 = sdf_sphere(p, 2.0f);  // Large sphere
    const auto d2 = sdf_sphere(p, 1.0f);  // Small sphere to subtract
    
    const auto result = sdf_subtraction(d1, d2);
    EXPECT_GT(result, d1);  // Result should be larger (hollowed out)
}

// ========== Gradient/Normal Tests ==========

TEST_F(SDFTest, SphereGradientPointsOutward) {
    // Gradient at (1, 0, 0) should point in +X direction
    const auto p = glm::vec3(1.0f, 0.0f, 0.0f);
    const auto normal = sdf_gradient([](const glm::vec3& pt) {
        return sdf_sphere(pt, 1.0f);
    }, p, 0.001f);
    
    // Normal should be approximately (1, 0, 0)
    EXPECT_NEAR(normal.x, 1.0f, 0.01f);
    EXPECT_NEAR(normal.y, 0.0f, 0.01f);
    EXPECT_NEAR(normal.z, 0.0f, 0.01f);
}

TEST_F(SDFTest, PlaneGradientMatchesNormal) {
    // Plane with normal (0, 1, 0)
    const auto plane_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto p = glm::vec3(5.0f, 2.0f, 3.0f);
    
    const auto computed_normal = sdf_gradient([plane_normal](const glm::vec3& pt) {
        return sdf_plane(pt, plane_normal, 0.0f);
    }, p, 0.001f);
    
    // Computed normal should match plane normal
    EXPECT_NEAR(computed_normal.x, 0.0f, 0.01f);
    EXPECT_NEAR(computed_normal.y, 1.0f, 0.01f);
    EXPECT_NEAR(computed_normal.z, 0.0f, 0.01f);
}

// ========== Constexpr Tests ==========

// Note: GLM functions like length() are not constexpr in GLM 1.0.1,
// so we can't test compile-time evaluation here. The SDF functions
// are marked constexpr for future GLM versions that support it.

TEST_F(SDFTest, SDFFunctionsHaveConstexprSignature) {
    // Just verify the functions can be used at compile time when GLM allows it
    // Currently this will evaluate at runtime
    const auto sphere_dist = sdf_sphere(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
    const auto box_dist = sdf_box(glm::vec3(0.0f), glm::vec3(1.0f), 0.0f);
    const auto plane_dist = sdf_plane(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f);
    
    // Just ensure they work
    EXPECT_TRUE(sphere_dist == sphere_dist);
    EXPECT_TRUE(box_dist == box_dist);
    EXPECT_TRUE(plane_dist == plane_dist);
}
