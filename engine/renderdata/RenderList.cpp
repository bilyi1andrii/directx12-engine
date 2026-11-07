#include "renderdata/RenderList.h"

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

    void SubmitGrid(RenderList &list, const BatchKey &key, const DirectX::XMFLOAT4X4 &base, uint32_t rows,
                    uint32_t cols, float spacingX, float spacingZ) {
        using namespace DirectX;

        const XMMATRIX baseMatrix = XMLoadFloat4x4(&base);
        // Grid centre in cells, so the grid is centred on the base position
        const float centreX = (static_cast<float>(cols) - 1.0f) * 0.5f;
        const float centreZ = (static_cast<float>(rows) - 1.0f) * 0.5f;

        XMFLOAT4X4 world{};
        for (uint32_t row = 0; row < rows; ++row) {
            for (uint32_t col = 0; col < cols; ++col) {
                const float x = (static_cast<float>(col) - centreX) * spacingX;
                const float z = (static_cast<float>(row) - centreZ) * spacingZ;
                // Row vectors: the offset is applied after `base`, i.e. in world space
                XMStoreFloat4x4(&world, baseMatrix * XMMatrixTranslation(x, 0.0f, z));
                list.Add(key, world);
            }
        }
    }
} // namespace engine::render
