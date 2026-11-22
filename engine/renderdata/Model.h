#ifndef D3D12_ENGINE_MODEL_H
#define D3D12_ENGINE_MODEL_H

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/MeshData.h"

namespace engine::render {
    // An axis-aligned box: the smallest corner and the largest one
    struct Aabb {
        DirectX::XMFLOAT3 min{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 max{0.0f, 0.0f, 0.0f};
    };

    // The box around the vertices `submesh` draws, in the submesh's own space. `submesh` must draw at least one
    // triangle.
    [[nodiscard]] Aabb SubmeshBounds(const MeshData &mesh, const SubmeshData &submesh);

    [[nodiscard]] inline DirectX::XMFLOAT3 Center(const Aabb &box) noexcept {
        return {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f};
    }

    // A drawable part of a model: submesh, material and place in the model
    struct ModelPart {
        MeshHandle mesh;
        MaterialHandle material;
        // Part -> model space, row-vector convention (MeshNode::transform). Identity by default.
        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
        Aabb bounds; // of the submesh, in its own space (before `transform`)
    };

    // A model on the GPU: parts and bounds. AssetCache makes it from ModelData; Scene::AddObject places it.
    struct Model {
        std::vector<ModelPart> parts;
        Aabb bounds;                // of all parts, in model space
        uint64_t triangleCount = 0; // of all parts
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MODEL_H
