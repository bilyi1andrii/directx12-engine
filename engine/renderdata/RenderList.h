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
#include "renderdata/Model.h"

namespace engine::render {
    // Objects with the same key can be drawn together: same mesh, same material
    struct BatchKey {
        MeshHandle mesh;
        MaterialHandle material;

        bool operator==(const BatchKey &) const = default;
    };

    // Hash of a BatchKey: both handle indices packed into one 64-bit value
    struct BatchKeyHash {
        [[nodiscard]] size_t operator()(const BatchKey &key) const noexcept {
            return std::hash<uint64_t>{}((uint64_t{key.mesh.index} << 32) | key.material.index);
        }
    };

    // Every object drawn with one key this frame
    struct BatchBucket {
        BatchKey key;
        // Object -> world, one per object. Not transposed (DirectXMath convention, like FrameView):
        // the renderer transposes while it writes them into the frame's InstanceData buffer.
        std::vector<DirectX::XMFLOAT4X4> worlds;
    };

    // The frame's draws as plain CPU data, without D3D12 types: objects grouped into buckets by BatchKey,
    // buckets in the order their key was first added. Rebuilt every frame for moving objects; objects that never
    // move go into GpuResources::CreateStaticList once.
    // Transparency comes later.
    //
    // The same list serves both draw paths: the renderer draws a bucket either as one instanced draw or
    // as one draw per object, depending on its settings.
    class RenderList {
    public:
        // Adds one object
        void Add(const BatchKey &key, const DirectX::XMFLOAT4X4 &world);

        // Removes every object and bucket. The buckets' memory is kept for the next frame; the key map's nodes are not.
        void Clear() noexcept;

        [[nodiscard]] std::span<const BatchBucket> Buckets() const noexcept { return m_buckets; }

        // Objects in all buckets: the number of InstanceData elements the renderer uploads
        [[nodiscard]] size_t InstanceCount() const noexcept { return m_instanceCount; }

    private:
        // The bucket for `key`, appended if there is none yet
        [[nodiscard]] BatchBucket &GetOrCreate(const BatchKey &key);

        std::vector<BatchBucket> m_buckets; // in first-added order
        // Key -> index in m_buckets, so finding a bucket does not search them all
        std::unordered_map<BatchKey, uint32_t, BatchKeyHash> m_bucketIndices;
        // The bucket of the previous Add: objects in a row often share a key, and then no hash is needed
        uint32_t m_lastBucket = 0;
        // Emptied `worlds` vectors from Clear(), reused by new buckets so they do not grow from zero again
        std::vector<std::vector<DirectX::XMFLOAT4X4>> m_spareWorlds;
        size_t m_instanceCount = 0;
    };

    // Adds rows x cols copies of a model on the XZ plane, centred on `base`. Each part of copy (row, col) gets
    // world = part.transform * base * translation(GridCellOffset(row, col, ...)).
    void SubmitGrid(RenderList &list, std::span<const ModelPart> parts, const DirectX::XMFLOAT4X4 &base, uint32_t rows,
                    uint32_t cols, float spacingX, float spacingZ);
} // namespace engine::render

#endif // D3D12_ENGINE_RENDERLIST_H
