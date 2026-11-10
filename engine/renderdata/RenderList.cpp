#include "renderdata/RenderList.h"

#include <utility>

#include "renderdata/GridLayout.h"

namespace engine::render {
    void RenderList::Add(const BatchKey &key, const DirectX::XMFLOAT4X4 &world) {
        GetOrCreate(key).worlds.push_back(world);
        ++m_instanceCount;
    }

    void RenderList::Clear() noexcept {
        // In reverse, so next frame's first bucket gets this frame's first vector back: with the same objects,
        // every vector is already big enough. GetOrCreate reserved the room, so push_back does not allocate.
        for (auto bucket = m_buckets.rbegin(); bucket != m_buckets.rend(); ++bucket) {
            bucket->worlds.clear();
            m_spareWorlds.push_back(std::move(bucket->worlds));
        }
        m_buckets.clear();
        m_bucketIndices.clear();
        m_lastBucket = 0;
        m_instanceCount = 0;
    }

    BatchBucket &RenderList::GetOrCreate(const BatchKey &key) {
        // Most often the same key as the previous object; otherwise look it up
        if (m_lastBucket < m_buckets.size() && m_buckets[m_lastBucket].key == key) {
            return m_buckets[m_lastBucket];
        }
        if (const auto found = m_bucketIndices.find(key); found != m_bucketIndices.end()) {
            m_lastBucket = found->second;
            return m_buckets[m_lastBucket];
        }

        // A new bucket, with a spare vector if there is one. Room for every vector in m_spareWorlds keeps
        // Clear() from allocating.
        m_spareWorlds.reserve(m_buckets.size() + m_spareWorlds.size() + 1);
        BatchBucket newBucket{.key = key, .worlds = {}};
        if (!m_spareWorlds.empty()) {
            newBucket.worlds = std::move(m_spareWorlds.back());
            m_spareWorlds.pop_back();
        }
        const auto index = static_cast<uint32_t>(m_buckets.size());
        BatchBucket &bucket = m_buckets.emplace_back(std::move(newBucket));
        try {
            m_bucketIndices.emplace(key, index);
        } catch (...) {
            // Without a map entry, the next Add would create a second bucket for this key
            m_buckets.pop_back();
            throw;
        }
        m_lastBucket = index;
        return bucket;
    }

    void SubmitGrid(RenderList &list, std::span<const ModelPart> parts, const DirectX::XMFLOAT4X4 &base, uint32_t rows,
                    uint32_t cols, float spacingX, float spacingZ) {
        using namespace DirectX;

        const XMMATRIX baseMatrix = XMLoadFloat4x4(&base);
        XMFLOAT4X4 world{};
        // One part at a time: its copies share a key, so RenderList::Add does not have to look up the bucket
        for (const ModelPart &part : parts) {
            const BatchKey key{.mesh = part.mesh, .material = part.material};
            const XMMATRIX partBase = XMLoadFloat4x4(&part.transform) * baseMatrix;
            for (uint32_t row = 0; row < rows; ++row) {
                for (uint32_t col = 0; col < cols; ++col) {
                    const XMFLOAT3 offset = GridCellOffset(row, col, rows, cols, spacingX, spacingZ);
                    // Row vectors: the offset is applied after `base`, i.e. in world space
                    XMStoreFloat4x4(&world, partBase * XMMatrixTranslation(offset.x, offset.y, offset.z));
                    list.Add(key, world);
                }
            }
        }
    }
} // namespace engine::render
