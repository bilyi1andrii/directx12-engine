#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/TransparentSort.h"

using engine::render::SortBackToFront;
using engine::render::SortPoint;
using engine::render::TransparentObject;
using engine::render::ViewDepth;

namespace {
    struct Object {
        DirectX::XMFLOAT3 position; // where its world matrix puts its origin
        float depth;                // expected ViewDepth of its sort point
        uint32_t instanceBuffer = 0;
        uint32_t element = 0;
        DirectX::XMFLOAT3 meshCenter{}; // in mesh space
        float turnDegrees = 0.0f;       // about the Y axis, before moving it to `position`
    };
} // namespace

// Each object is sorted by the middle of its mesh, placed by its world matrix. Its depth is measured along the
// camera's direction, also for a camera on an axis or tilted, and for objects behind it. The farthest is drawn first;
// at equal depths the instance buffer, then the element decides.
TEST(TransparentSort, SortBackToFrontByViewDepth) {
    struct Case {
        std::string_view name;
        DirectX::XMFLOAT3 eye;
        DirectX::XMFLOAT3 target;
        std::vector<Object> objects;
        std::vector<size_t> drawOrder; // indices into `objects`
    };
    const float diagonal = std::sqrt(2.0f);
    const Case kCases[] = {
        {"looking along +Z",
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {{{0.0f, 0.0f, 2.0f}, 2.0f, 0, 0}, {{0.0f, 0.0f, 10.0f}, 10.0f, 0, 1}, {{3.0f, 4.0f, 5.0f}, 5.0f, 0, 2}},
         {1, 2, 0}},
        {"on the X axis, looking back at the origin",
         {10.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {{{0.0f, 0.0f, 0.0f}, 10.0f, 0, 0}, {{5.0f, 0.0f, 3.0f}, 5.0f, 0, 1}, {{-5.0f, 0.0f, 0.0f}, 15.0f, 0, 2}},
         {2, 0, 1}},
        {"tilted down at the origin",
         {0.0f, 10.0f, -10.0f},
         {0.0f, 0.0f, 0.0f},
         {{{0.0f, 0.0f, 0.0f}, 10.0f * diagonal, 0, 0},
          {{0.0f, 0.0f, 10.0f}, 15.0f * diagonal, 0, 1},
          {{0.0f, 5.0f, -5.0f}, 5.0f * diagonal, 0, 2}},
         {1, 0, 2}},
        {"objects behind the camera",
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {{{0.0f, 0.0f, -3.0f}, -3.0f, 0, 0}, {{0.0f, 0.0f, 1.0f}, 1.0f, 0, 1}, {{2.0f, 0.0f, -1.0f}, -1.0f, 0, 2}},
         {1, 2, 0}},
        {"equal depths",
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {{{1.0f, 0.0f, 5.0f}, 5.0f, 2, 0}, {{-1.0f, 0.0f, 5.0f}, 5.0f, 1, 7}, {{0.0f, 2.0f, 5.0f}, 5.0f, 1, 3}},
         {2, 1, 0}},
        // Three parts of one model, one behind the other, seen from the side of the first: by their origin they
        // would tie and keep their order
        {"parts of one model at one origin",
         {0.0f, 0.0f, -10.0f},
         {0.0f, 0.0f, 0.0f},
         {{{0.0f, 0.0f, 0.0f}, 10.0f, 0, 0, {1.0f, 1.0f, 0.0f}},
          {{0.0f, 0.0f, 0.0f}, 10.5f, 0, 1, {1.5f, 1.5f, 0.5f}},
          {{0.0f, 0.0f, 0.0f}, 11.0f, 0, 2, {2.0f, 2.0f, 1.0f}}},
         {2, 1, 0}},
        // A quarter turn moves the mesh's middle from its +X side towards the camera
        {"a turned object",
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {{{0.0f, 0.0f, 5.0f}, 4.0f, 0, 0, {1.0f, 0.0f, 0.0f}, 90.0f}, {{0.0f, 0.0f, 4.5f}, 4.5f, 0, 1}},
         {1, 0}},
    };
    for (const Case &test : kCases) {
        using namespace DirectX;
        XMFLOAT4X4 view{};
        XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMLoadFloat3(&test.eye), XMLoadFloat3(&test.target),
                                                XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
        std::vector<TransparentObject> sorted;
        for (const Object &object : test.objects) {
            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world, XMMatrixRotationY(XMConvertToRadians(object.turnDegrees)) *
                                        XMMatrixTranslation(object.position.x, object.position.y, object.position.z));
            const float depth = ViewDepth(view, SortPoint(world, object.meshCenter));
            EXPECT_NEAR(depth, object.depth, 1e-4f) << test.name;
            sorted.push_back(
                {.instanceBufferIndex = object.instanceBuffer, .element = object.element, .viewDepth = depth});
        }

        SortBackToFront(sorted);

        ASSERT_EQ(sorted.size(), test.drawOrder.size()) << test.name;
        for (size_t i = 0; i < sorted.size(); ++i) {
            const Object &expected = test.objects[test.drawOrder[i]];
            EXPECT_EQ(sorted[i].instanceBufferIndex, expected.instanceBuffer) << test.name << ", draw " << i;
            EXPECT_EQ(sorted[i].element, expected.element) << test.name << ", draw " << i;
        }
    }
}
