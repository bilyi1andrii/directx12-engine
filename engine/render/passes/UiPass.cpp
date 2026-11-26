#include "render/passes/UiPass.h"

#include <optional>

#include "render/FrameInputs.h"
#include "render/UiRenderer.h"

namespace render {
    void AddUiPass(FrameGraph &graph, FgTexture target, uint32_t view, UiRenderer &ui) {
        graph.AddPass(
            "UI",
            [&](PassBuilder &builder) {
                // No clear: the UI goes on top of the scene
                builder.WriteColor(target, std::nullopt, view);
            },
            [&ui](PassContext &context) {
                if (context.Inputs().ui != nullptr) {
                    ui.Render(context.Inputs().ui, context.Commands(), context.Frame());
                }
            });
    }
} // namespace render
