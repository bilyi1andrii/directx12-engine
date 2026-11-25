// Tests the order see-through objects are drawn in: from the farthest to the nearest along the camera's view, by the
// middle of each mesh, with a fixed order for objects at the same depth.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/TransparentSort.h"

using render::SortBackToFront;
using render::SortPoint;
using render::TransparentObject;
using render::ViewDepth;

namespace {
    struct Object {
        DirectX::XMFLOAT3 position;
        float depth;
        uint32_t instanceBuffer = 0;
        uint32_t element = 0;
        DirectX::XMFLOAT3 meshCenter{};
    };
} // namespace

TEST(TransparentSort, SortBackToFrontByViewDepth) {
    struct Case {
        std::string_view name;
        DirectX::XMFLOAT3 eye;
        DirectX::XMFLOAT3 target;
        std::vector<Object> objects;
        std::vector<size_t> drawOrder;
    };
    const Case kCases[] = {
        {"on the X axis, looking back at the origin",
         {10.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {{{0.0f, 0.0f, 0.0f}, 10.0f, 0, 0}, {{5.0f, 0.0f, 3.0f}, 5.0f, 0, 1}, {{-5.0f, 0.0f, 0.0f}, 15.0f, 0, 2}},
         {2, 0, 1}},
        {"equal depths",
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {{{1.0f, 0.0f, 5.0f}, 5.0f, 2, 0}, {{-1.0f, 0.0f, 5.0f}, 5.0f, 1, 7}, {{0.0f, 2.0f, 5.0f}, 5.0f, 1, 3}},
         {2, 1, 0}},
        // Sorted by their origin, these three would tie
        {"parts of one model at one origin",
         {0.0f, 0.0f, -10.0f},
         {0.0f, 0.0f, 0.0f},
         {{{0.0f, 0.0f, 0.0f}, 10.0f, 0, 0, {1.0f, 1.0f, 0.0f}},
          {{0.0f, 0.0f, 0.0f}, 10.5f, 0, 1, {1.5f, 1.5f, 0.5f}},
          {{0.0f, 0.0f, 0.0f}, 11.0f, 0, 2, {2.0f, 2.0f, 1.0f}}},
         {2, 1, 0}},
    };
    for (const Case &test : kCases) {
        using namespace DirectX;
        XMFLOAT4X4 view{};
        XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMLoadFloat3(&test.eye), XMLoadFloat3(&test.target),
                                                XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
        std::vector<TransparentObject> sorted;
        for (const Object &object : test.objects) {
            XMFLOAT4X4 world{};
            XMStoreFloat4x4(&world, XMMatrixTranslation(object.position.x, object.position.y, object.position.z));
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
