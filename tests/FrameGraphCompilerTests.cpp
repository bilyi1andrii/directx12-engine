#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "render/FrameGraphCompiler.h"

using engine::render::fg::Access;
using engine::render::fg::Compile;
using engine::render::fg::CompiledGraph;
using engine::render::fg::GraphDesc;
using engine::render::fg::kNoIndex;
using engine::render::fg::TextureUse;
using engine::render::fg::Transition;

namespace {
    // Description ids of the transients in these tests: equal ids mean the same size, format and usage
    constexpr uint32_t kSceneDepthDesc = 0;
    constexpr uint32_t kShadowMapDesc = 1;
    constexpr uint32_t kColorDesc = 2;

    uint32_t AddPass(GraphDesc &graph, std::initializer_list<TextureUse> uses) {
        return graph.AddPass(std::span(uses.begin(), uses.size()));
    }

    [[nodiscard]] std::vector<Transition> Before(const CompiledGraph &compiled, uint32_t pass) {
        const std::span<const Transition> transitions = compiled.TransitionsBefore(pass);
        return {transitions.begin(), transitions.end()};
    }

    [[nodiscard]] uint32_t Gpu(const CompiledGraph &compiled, uint32_t texture) {
        return compiled.textures[texture].gpuTexture;
    }
} // namespace

TEST(FrameGraphCompiler, SinglePassToTheBackBuffer) {
    // The back buffer goes to ColorWrite before the pass and back to Present after the frame. Scene depth is only
    // ever DepthWrite, so it needs no transition, not even across frames.
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t depth = graph.AddTransient(kSceneDepthDesc);
    const uint32_t opaque = AddPass(graph, {{backBuffer, Access::ColorWrite}, {depth, Access::DepthWrite}});

    const CompiledGraph compiled = Compile(graph);

    EXPECT_TRUE(compiled.passes[opaque].kept);
    ASSERT_EQ(compiled.gpuTextures.size(), 2u);
    const uint32_t bb = Gpu(compiled, backBuffer);
    EXPECT_TRUE(compiled.gpuTextures[bb].imported);
    EXPECT_EQ(compiled.gpuTextures[Gpu(compiled, depth)].restAccess, Access::DepthWrite);
    EXPECT_EQ(Before(compiled, opaque), (std::vector<Transition>{{bb, Access::Present, Access::ColorWrite}}));
    EXPECT_EQ(compiled.finalTransitions, (std::vector<Transition>{{bb, Access::ColorWrite, Access::Present}}));
}

TEST(FrameGraphCompiler, ShadowOpaqueUi) {
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t shadowMap = graph.AddTransient(kShadowMapDesc);
    const uint32_t depth = graph.AddTransient(kSceneDepthDesc);
    const uint32_t shadow = AddPass(graph, {{shadowMap, Access::DepthWrite}});
    const uint32_t opaque = AddPass(
        graph, {{shadowMap, Access::ShaderRead}, {backBuffer, Access::ColorWrite}, {depth, Access::DepthWrite}});
    const uint32_t ui = AddPass(graph, {{backBuffer, Access::ColorWrite}});

    const CompiledGraph compiled = Compile(graph);

    EXPECT_TRUE(compiled.passes[shadow].kept);
    EXPECT_TRUE(compiled.passes[opaque].kept);
    EXPECT_TRUE(compiled.passes[ui].kept);
    EXPECT_EQ(compiled.textures[shadowMap].firstPass, shadow);
    EXPECT_EQ(compiled.textures[shadowMap].lastPass, opaque);
    EXPECT_EQ(compiled.textures[depth].firstPass, opaque);
    EXPECT_EQ(compiled.textures[depth].lastPass, opaque);
    EXPECT_EQ(compiled.textures[backBuffer].firstPass, opaque);
    EXPECT_EQ(compiled.textures[backBuffer].lastPass, ui);

    // The shadow map ends each frame sampled, so the next frame starts from there
    const uint32_t sm = Gpu(compiled, shadowMap);
    const uint32_t bb = Gpu(compiled, backBuffer);
    EXPECT_EQ(compiled.gpuTextures[sm].restAccess, Access::ShaderRead);
    EXPECT_EQ(Before(compiled, shadow), (std::vector<Transition>{{sm, Access::ShaderRead, Access::DepthWrite}}));
    EXPECT_EQ(Before(compiled, opaque), (std::vector<Transition>{{sm, Access::DepthWrite, Access::ShaderRead},
                                                                 {bb, Access::Present, Access::ColorWrite}}));
    EXPECT_TRUE(Before(compiled, ui).empty());
    EXPECT_EQ(compiled.finalTransitions, (std::vector<Transition>{{bb, Access::ColorWrite, Access::Present}}));
}

TEST(FrameGraphCompiler, TextureReadByTwoPassesChangesStateOnce) {
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t shadowMap = graph.AddTransient(kShadowMapDesc);
    const uint32_t shadow = AddPass(graph, {{shadowMap, Access::DepthWrite}});
    const uint32_t opaque = AddPass(graph, {{shadowMap, Access::ShaderRead}, {backBuffer, Access::ColorWrite}});
    const uint32_t transparent = AddPass(graph, {{shadowMap, Access::ShaderRead}, {backBuffer, Access::ColorWrite}});

    const CompiledGraph compiled = Compile(graph);

    const uint32_t sm = Gpu(compiled, shadowMap);
    EXPECT_EQ(compiled.textures[shadowMap].lastPass, transparent);
    EXPECT_EQ(Before(compiled, shadow), (std::vector<Transition>{{sm, Access::ShaderRead, Access::DepthWrite}}));
    EXPECT_EQ(Before(compiled, opaque).front(), (Transition{sm, Access::DepthWrite, Access::ShaderRead}));
    EXPECT_TRUE(Before(compiled, transparent).empty());
}

TEST(FrameGraphCompiler, PassWhoseOutputNobodyUsesIsCulled) {
    // `debugView` writes a texture nobody reads, so it does not run. `prepass` writes a texture that only
    // `debugView` reads, so it does not run either.
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t prepassTarget = graph.AddTransient(kColorDesc);
    const uint32_t debugTarget = graph.AddTransient(kColorDesc);
    const uint32_t prepass = AddPass(graph, {{prepassTarget, Access::ColorWrite}});
    const uint32_t debugView = AddPass(graph, {{prepassTarget, Access::ShaderRead}, {debugTarget, Access::ColorWrite}});
    const uint32_t opaque = AddPass(graph, {{backBuffer, Access::ColorWrite}});

    const CompiledGraph compiled = Compile(graph);

    EXPECT_FALSE(compiled.passes[prepass].kept);
    EXPECT_FALSE(compiled.passes[debugView].kept);
    EXPECT_TRUE(compiled.passes[opaque].kept);
    EXPECT_TRUE(Before(compiled, prepass).empty());
    EXPECT_TRUE(Before(compiled, debugView).empty());
    // Culled passes' textures get no GPU texture
    EXPECT_EQ(compiled.textures[prepassTarget].gpuTexture, kNoIndex);
    EXPECT_EQ(compiled.textures[debugTarget].firstPass, kNoIndex);
    EXPECT_EQ(compiled.gpuTextures.size(), 1u);
}

TEST(FrameGraphCompiler, EarlierWriterOfATextureALaterPassWritesIsKept) {
    // `ui` may draw on top of what `opaque` wrote (no clear), so `opaque` runs even though no pass reads the
    // back buffer
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t overlay = graph.AddTransient(kColorDesc);
    const uint32_t opaque = AddPass(graph, {{overlay, Access::ColorWrite}});
    const uint32_t ui = AddPass(graph, {{overlay, Access::ColorWrite}, {backBuffer, Access::ColorWrite}});

    const CompiledGraph compiled = Compile(graph);

    EXPECT_TRUE(compiled.passes[opaque].kept);
    EXPECT_TRUE(compiled.passes[ui].kept);
    EXPECT_EQ(compiled.textures[overlay].firstPass, opaque);
}

TEST(FrameGraphCompiler, TransientsShareATextureOnlyWhenLifetimesDoNotOverlapAndDescriptionsMatch) {
    // Two post-processing steps, each writing a colour target and reading it into the back buffer.
    // `first` and `second` have the same description and never live at the same time: one GPU texture.
    // `other` has another description, and `firstAgain` overlaps `first`: each gets its own.
    GraphDesc graph;
    const uint32_t backBuffer = graph.AddImport(Access::Present, Access::Present);
    const uint32_t first = graph.AddTransient(kColorDesc);
    const uint32_t firstAgain = graph.AddTransient(kColorDesc);
    const uint32_t second = graph.AddTransient(kColorDesc);
    const uint32_t other = graph.AddTransient(kShadowMapDesc);
    const uint32_t pass0 = AddPass(graph, {{first, Access::ColorWrite}, {firstAgain, Access::ColorWrite}});
    const uint32_t pass1 = AddPass(
        graph, {{first, Access::ShaderRead}, {firstAgain, Access::ShaderRead}, {backBuffer, Access::ColorWrite}});
    const uint32_t pass2 = AddPass(graph, {{second, Access::ColorWrite}, {other, Access::DepthWrite}});
    const uint32_t pass3 =
        AddPass(graph, {{second, Access::ShaderRead}, {other, Access::ShaderRead}, {backBuffer, Access::ColorWrite}});

    const CompiledGraph compiled = Compile(graph);

    const uint32_t shared = Gpu(compiled, first);
    EXPECT_EQ(Gpu(compiled, second), shared);
    EXPECT_NE(Gpu(compiled, firstAgain), shared);
    EXPECT_NE(Gpu(compiled, other), shared);
    EXPECT_NE(Gpu(compiled, other), Gpu(compiled, firstAgain));
    EXPECT_EQ(compiled.gpuTextures.size(), 4u); // back buffer, shared, firstAgain, other

    // The shared texture's uses form one loop over the frame: written, read, written, read
    EXPECT_EQ(compiled.gpuTextures[shared].restAccess, Access::ShaderRead);
    EXPECT_EQ(Before(compiled, pass0).front(), (Transition{shared, Access::ShaderRead, Access::ColorWrite}));
    EXPECT_EQ(Before(compiled, pass1).front(), (Transition{shared, Access::ColorWrite, Access::ShaderRead}));
    EXPECT_EQ(Before(compiled, pass2).front(), (Transition{shared, Access::ShaderRead, Access::ColorWrite}));
    EXPECT_EQ(Before(compiled, pass3).front(), (Transition{shared, Access::ColorWrite, Access::ShaderRead}));
}
