# PS5VK-Overlay

A small, reusable Dear ImGui information-panel renderer for PS5 Vulkan projects. This is a standalone copy of the rendering component from `UnleashedRecomp-Prospero`; the original game project is not modified by this bundle.

**Import target:** `PS5VK::Overlay`  
**Public header:** `<ps5vk_overlay.h>`  
**C++ requirement:** C++17  
**External dependency:** your project's existing Dear ImGui build/context

The overlay itself does not initialize Vulkan, Dear ImGui, Prospero APIs, fonts, or a game runtime. It adds draw commands to the current ImGui background draw list. The host project supplies the metrics and labels, so the renderer can be reused without carrying over Sonic/game-specific globals.

## Quick import with `add_subdirectory`

Copy this folder into a project such as `thirdparty/PS5VK-Overlay/`. Define or locate your Dear ImGui target **before** adding this directory:

```cmake
add_subdirectory(thirdparty/imgui) # if your project builds ImGui here
add_subdirectory(thirdparty/PS5VK-Overlay)
target_link_libraries(MyGame PRIVATE PS5VK::Overlay)
```

If your ImGui target is not named `imgui` or `imgui::imgui`, specify it when configuring CMake:

```bash
cmake -S . -B build \
  -DPS5VK_OVERLAY_IMGUI_TARGET=my_imgui_target
```

Alternatively, if ImGui is supplied through include/library paths rather than a CMake target, set its header directory:

```bash
cmake -S . -B build \
  -DPS5VK_OVERLAY_IMGUI_INCLUDE_DIR=/path/to/imgui
```

In that case, also link your app against the same Dear ImGui library that provides the symbols used by the overlay.

## Draw it in an existing ImGui frame

Call `Draw()` after `ImGui::NewFrame()` and before `ImGui::Render()`. Supply values collected by your own game/runtime; pass already-localized text if the project supports multiple languages.

```cpp
#include <ps5vk_overlay.h>

#include <vector>

void DrawSystemInfo(ImFont* font)
{
    const std::vector<ps5vk_overlay::Section> sections{
        { "PERFORMANCE", { "FPS: 60.0", "CPU update: 3.10 ms", "GPU frame: 5.42 ms" } },
        { "GPU", { "AMD Oberon / Vulkan" } },
        { "MEMORY", { "Game heap: 132.0/256.0 MiB (peak 161.0)" } },
        { "DISPLAY", { "Render resolution: 1920x1080" } },
        { "PATHS", { "User data: /data/MyGame" } },
    };

    ps5vk_overlay::Style style;
    style.position = { 14.0f, 14.0f };
    style.fontSize = 10.0f;
    style.padding = 8.0f;
    style.lineHeight = 14.0f;
    style.sectionFontSize = 8.0f;
    style.title = "MY PS5 APP";
    style.titleFontSize = 13.0f;
    style.accentColor = IM_COL32(0, 120, 247, 235);
    style.titleColor = IM_COL32(0, 120, 247, 255);
    style.sectionTitleColor = IM_COL32(0, 120, 247, 255);
    style.accentFirstLine = true;

    ps5vk_overlay::Draw(sections, font, style);
}
```

The caller still owns the actual ImGui/Vulkan frame setup and renderer backend:

```cpp
ImGui::NewFrame();
DrawSystemInfo(gameFont);
ImGui::Render();
// Submit ImGui draw data with the project's existing Vulkan/PS5 backend.
```

`ps5vk_overlay::palette::kColors` provides the optional 256-color palette copied from the source project's theme helpers. Use `palette::ClampIndex()` to guard a saved selection.

## Included / intentionally not included

Included:

- compact top-left card with optional title;
- section headings, rows, separators, border, shadow, and accent stripe;
- configurable colors, sizing, and accent-first-row styling;
- optional 256-color accent palette;
- CMake target for easy source integration.

The following remain with the host project: FPS/CPU/GPU timing collection, GPU name, PS5 temperature or sensor APIs, heaps/allocator stats, resolution, executable/user-data paths, configuration/menu wiring, and translations. In particular, the full `DrawSystemInfo()` function in UnleashedRecomp refers to that game’s `Config`, logger, profiler, GPU device, O1Heap instances, path service, and localization system; those pieces are deliberately **not** copied into this portable renderer. A PS5 project can collect its own data (including Prospero API readings) and pass the formatted rows to this library.

No direct Vulkan or Sony SDK symbols are used by the library, so the same renderer can be compiled against any ImGui build that your PS5 Vulkan application already uses. The actual ImGui Vulkan backend and the console toolchain remain your project’s responsibility.

## Source and license

`system_info_overlay.h/.cpp` and the palette are copied from the workspace's `UnleashedRecomp-Prospero` project. The included `COPYING` contains the GNU GPL version 3 license. Keep the license with redistributed copies and check GPL compatibility for the project that imports it—particularly if that project is closed-source.
