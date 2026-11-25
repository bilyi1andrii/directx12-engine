#ifndef D3D12_ENGINE_MODEL_H
#define D3D12_ENGINE_MODEL_H

// A model ready to draw: a list of parts, each a piece of mesh with its own material and place in the model, and a
// box around the whole model and around each part. A model is built once when its file loads, then placed in the
// scene as many times as needed.

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/MeshData.h"

namespace engine::render {
    struct Aabb {
        DirectX::XMFLOAT3 min{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 max{0.0f, 0.0f, 0.0f};
    };

    [[nodiscard]] Aabb SubmeshBounds(const MeshData &mesh, const SubmeshData &submesh);

    [[nodiscard]] inline DirectX::XMFLOAT3 Center(const Aabb &box) noexcept {
        return {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f};
    }

    struct ModelPart {
        MeshHandle mesh;
        MaterialHandle material;

        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
        Aabb bounds;
    };

    struct Model {
        std::vector<ModelPart> parts;
        Aabb bounds;
        uint64_t triangleCount = 0;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MODEL_H