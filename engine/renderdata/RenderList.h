#ifndef D3D12_ENGINE_RENDERLIST_H
#define D3D12_ENGINE_RENDERLIST_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/Handles.h"

namespace engine::render {
    // Objects with the same key can be drawn together: same mesh, same material
    // (DX11 GeometrySystem BatchKey, with handles instead of pointers)
    struct BatchKey {
        MeshHandle mesh;
        MaterialHandle material;

        bool operator==(const BatchKey &) const = default;
    };

    // Hash of a BatchKey for unordered containers: both handle indices packed into one 64-bit value
    struct BatchKeyHash {
        [[nodiscard]] size_t operator()(const BatchKey &key) const noexcept {
            return std::hash<uint64_t>{}((uint64_t{key.mesh.index} << 32) | key.material.index);
        }
    };

    // Every object drawn with one key this frame (DX11 BatchBucket)
    struct BatchBucket {
        BatchKey key;
        // Object -> world, one per object. Not transposed (DirectXMath convention, like FrameView):
        // the renderer transposes while it writes them into the frame's InstanceData buffer.
        std::vector<DirectX::XMFLOAT4X4> worlds;
    };

    // The frame's draws as plain CPU data, without D3D12 types: objects grouped into buckets by BatchKey,
    // buckets in the order their key was first added. Port of the DX11 GeometrySystem bucket logic for
    // dynamic objects (rebuilt every frame); static buckets and transparency come later.
    //
    // The same list serves both draw paths: the renderer draws a bucket either as one instanced draw or
    // as one draw per object, depending on its settings.
    class RenderList {
    public:
        // Adds one object
        void Add(const BatchKey &key, const DirectX::XMFLOAT4X4 &world);

        // Removes every object and bucket (DX11 clearDynamic). Keeps their memory: the next frame's buckets reuse
        // it, so rebuilding a list of the same size allocates nothing.
        void Clear() noexcept;

        [[nodiscard]] std::span<const BatchBucket> Buckets() const noexcept { return m_buckets; }

        // Objects in all buckets: the number of InstanceData elements the renderer uploads
        [[nodiscard]] size_t InstanceCount() const noexcept { return m_instanceCount; }

    private:
        // The bucket for `key`, appended if there is none yet (DX11 getOrCreate, which searched the buckets)
        [[nodiscard]] BatchBucket &GetOrCreate(const BatchKey &key);

        std::vector<BatchBucket> m_buckets;
        // Where each key's bucket is in m_buckets, so finding it does not depend on the number of buckets
        // (a linear search over 400 keys cost about 2.9 ms per frame for 50k objects)
        std::unordered_map<BatchKey, uint32_t, BatchKeyHash> m_bucketIndices;
        // The bucket the previous Add used: consecutive objects often share a key (grids, copies of a model),
        // and then not even the hash is needed
        uint32_t m_lastBucket = 0;
        // Emptied `worlds` vectors of cleared buckets, with their capacity: GetOrCreate hands them to new buckets
        // (50k objects into fresh vectors cost about 2 ms per frame, mostly regrowth)
        std::vector<std::vector<DirectX::XMFLOAT4X4>> m_spareWorlds;
        size_t m_instanceCount = 0;
    };

    // Adds rows x cols copies of `key` on the XZ plane, centred on `base`: copy (row, col) gets
    // world = base * translation((col - (cols - 1) / 2) * spacingX, 0, (row - (rows - 1) / 2) * spacingZ).
    // DX11 GeometrySystem::submitGrid with one mesh; the per-mesh node transform it also applies
    // (mesh->transformMatrix) comes with model loading.
    void SubmitGrid(RenderList &list, const BatchKey &key, const DirectX::XMFLOAT4X4 &base, uint32_t rows,
                    uint32_t cols, float spacingX, float spacingZ);
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERLIST_H
