#include "renderdata/Model.h"

#include <cfloat>
#include <cstdint>

namespace render {
    Aabb SubmeshBounds(const MeshData &mesh, const SubmeshData &submesh) {
        using namespace DirectX;

        XMVECTOR low = XMVectorReplicate(FLT_MAX);
        XMVECTOR high = XMVectorReplicate(-FLT_MAX);
        for (uint32_t i = submesh.firstIndex; i < submesh.firstIndex + submesh.indexCount; ++i) {
            const XMVECTOR position = XMLoadFloat3(&mesh.vertices[submesh.baseVertex + mesh.indices[i]].position);
            low = XMVectorMin(low, position);
            high = XMVectorMax(high, position);
        }
        Aabb bounds;
        XMStoreFloat3(&bounds.min, low);
        XMStoreFloat3(&bounds.max, high);
        return bounds;
    }
} // namespace render
