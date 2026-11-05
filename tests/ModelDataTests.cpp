#include <variant>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/ModelData.h"

using engine::render::AlphaMode;
using engine::render::MaterialDesc;
using engine::render::MeshNode;
using engine::render::TextureSource;

// The defaults the importer relies on when a file leaves something out

TEST(MeshNode, DefaultTransformIsIdentity) {
    const MeshNode node;

    DirectX::XMFLOAT4X4 identity{};
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            EXPECT_EQ(node.transform(row, col), identity(row, col)) << "row " << row << ", col " << col;
        }
    }
}

TEST(MaterialDesc, DefaultIsOpaqueWhiteAndUntextured) {
    const MaterialDesc material;

    EXPECT_EQ(material.alphaMode, AlphaMode::Opaque);
    EXPECT_FALSE(material.doubleSided);
    EXPECT_EQ(material.baseColor.x, 1.0f);
    EXPECT_EQ(material.baseColor.y, 1.0f);
    EXPECT_EQ(material.baseColor.z, 1.0f);
    EXPECT_EQ(material.baseColor.w, 1.0f); // fully opaque
    EXPECT_FALSE(material.packedMetallicRoughness);
    for (const TextureSource *source :
         {&material.albedo, &material.normal, &material.emissive, &material.height, &material.opacityMap,
          &material.metallic, &material.roughness, &material.ambientOcclusion}) {
        EXPECT_TRUE(std::holds_alternative<std::monostate>(*source));
    }
}
