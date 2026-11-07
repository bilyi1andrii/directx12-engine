#ifndef D3D12_ENGINE_MODEL_H
#define D3D12_ENGINE_MODEL_H

#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"

namespace engine::render {
    // An axis-aligned box: the smallest corner and the largest one
    struct Aabb {
        DirectX::XMFLOAT3 min{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 max{0.0f, 0.0f, 0.0f};
    };

    // A drawable part of a model: submesh, material and place in the model
    struct ModelPart {
        MeshHandle mesh;
        MaterialHandle material;
        // Part -> model space, row-vector convention (MeshNode::transform). Identity by default.
        DirectX::XMFLOAT4X4 transform{1.0f, 0.0f, 0.0f, 0.0f, //
                                      0.0f, 1.0f, 0.0f, 0.0f, //
                                      0.0f, 0.0f, 1.0f, 0.0f, //
                                      0.0f, 0.0f, 0.0f, 1.0f};
    };

    // A model on the GPU: parts and bounds. AssetCache makes it from ModelData; Scene::Instantiate places it.
    struct Model {
        std::vector<ModelPart> parts;
        Aabb bounds; // of all parts, in model space
    };
} // namespace engine::render

#endif // D3D12_ENGINE_MODEL_H
