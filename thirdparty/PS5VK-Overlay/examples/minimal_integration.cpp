// Drop-in example only; call this while the host project's ImGui frame is active.
#include <ps5vk_overlay.h>

#include <string>
#include <vector>

void DrawExamplePS5VKOverlay(ImFont* font, double fps, double gpuFrameMs)
{
    std::vector<ps5vk_overlay::Section> sections{
        { "PERFORMANCE", {
            "FPS: " + std::to_string(fps),
            "GPU frame: " + std::to_string(gpuFrameMs) + " ms",
        } },
        { "PLATFORM", { "PS5 / Vulkan" } },
        { "MEMORY", { "Replace this row with your game's heap statistics" } },
    };

    ps5vk_overlay::Style style;
    style.position = { 14.0f, 14.0f };
    style.title = "MY PS5 APP";
    style.titleFontSize = 13.0f;
    style.accentColor = IM_COL32(0, 120, 247, 235);
    style.titleColor = IM_COL32(0, 120, 247, 255);
    style.sectionTitleColor = IM_COL32(0, 120, 247, 255);
    style.accentFirstLine = true;

    ps5vk_overlay::Draw(sections, font, style);
}
