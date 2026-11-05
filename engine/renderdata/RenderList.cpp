#include "renderdata/RenderList.h"

namespace engine::render {
    void RenderList::Add(const BatchKey &key, const DirectX::XMFLOAT4X4 &world) {
        GetOrCreate(key).worlds.push_back(world);
        ++m_instanceCount;
    }

    void RenderList::Clear() noexcept {
        m_buckets.clear();
        m_instanceCount = 0;
    }

    BatchBucket &RenderList::GetOrCreate(const BatchKey &key) {
        for (BatchBucket &bucket : m_buckets) {
            if (bucket.key == key) {
                return bucket;
            }
        }
        return m_buckets.emplace_back(BatchBucket{.key = key, .worlds = {}});
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
