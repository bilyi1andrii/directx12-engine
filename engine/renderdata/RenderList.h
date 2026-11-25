#ifndef D3D12_ENGINE_RENDERLIST_H
#define D3D12_ENGINE_RENDERLIST_H

// The frame's objects as plain data, grouped by mesh and material, so each group can be drawn together. It is
// rebuilt every frame and keeps its memory from one frame to the next.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"
#include "renderdata/Model.h"

namespace render {
    struct BatchKey {
        MeshHandle mesh;
        MaterialHandle material;

        bool operator==(const BatchKey &) const = default;
    };

    struct BatchKeyHash {
        [[nodiscard]] size_t operator()(const BatchKey &key) const noexcept {
            return std::hash<uint64_t>{}((uint64_t{key.mesh.index} << 32) | key.material.index);
        }
    };

    struct BatchBucket {
        BatchKey key;
        std::vector<DirectX::XMFLOAT4X4> worlds;
    };

    class RenderList {
    public:
        void Add(const BatchKey &key, const DirectX::XMFLOAT4X4 &world);

        void Clear() noexcept;

        [[nodiscard]] std::span<const BatchBucket> Buckets() const noexcept { return m_buckets; }

        [[nodiscard]] size_t InstanceCount() const noexcept { return m_instanceCount; }

    private:
        [[nodiscard]] BatchBucket &GetOrCreate(const BatchKey &key);

        std::vector<BatchBucket> m_buckets;
        std::unordered_map<BatchKey, uint32_t, BatchKeyHash> m_bucketIndices;
        uint32_t m_lastBucket = 0;
        std::vector<std::vector<DirectX::XMFLOAT4X4>> m_spareWorlds;
        size_t m_instanceCount = 0;
    };

    void SubmitGrid(RenderList &list, std::span<const ModelPart> parts, const DirectX::XMFLOAT4X4 &base, uint32_t rows,
                    uint32_t cols, float spacingX, float spacingZ);
} // namespace render

#endif // D3D12_ENGINE_RENDERLIST_H
