cmake_minimum_required(VERSION 3.20)
project(PS5VKOverlay VERSION 0.1.0 LANGUAGES CXX)

set(PS5VK_OVERLAY_IMGUI_TARGET "" CACHE STRING
    "Existing ImGui CMake target (for example imgui or imgui::imgui)")
set(PS5VK_OVERLAY_IMGUI_INCLUDE_DIR "" CACHE PATH
    "Directory containing imgui.h when the host does not expose an ImGui CMake target")

add_library(ps5vk_overlay STATIC
    src/system_info_overlay.cpp
)
add_library(PS5VK::Overlay ALIAS ps5vk_overlay)
set_target_properties(ps5vk_overlay PROPERTIES
    CXX_EXTENSIONS OFF
)
target_compile_features(ps5vk_overlay PUBLIC cxx_std_17)
target_include_directories(ps5vk_overlay PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(NOT PS5VK_OVERLAY_IMGUI_TARGET)
    if(TARGET imgui::imgui)
        set(PS5VK_OVERLAY_IMGUI_TARGET imgui::imgui)
    elseif(TARGET imgui)
        set(PS5VK_OVERLAY_IMGUI_TARGET imgui)
    endif()
endif()

if(PS5VK_OVERLAY_IMGUI_TARGET)
    if(NOT TARGET ${PS5VK_OVERLAY_IMGUI_TARGET})
        message(FATAL_ERROR
            "PS5VK_OVERLAY_IMGUI_TARGET='${PS5VK_OVERLAY_IMGUI_TARGET}' is not a known CMake target. "
            "Define ImGui before add_subdirectory(PS5VK-Overlay), or set PS5VK_OVERLAY_IMGUI_INCLUDE_DIR.")
    endif()
    target_link_libraries(ps5vk_overlay PUBLIC ${PS5VK_OVERLAY_IMGUI_TARGET})
elseif(PS5VK_OVERLAY_IMGUI_INCLUDE_DIR)
    if(NOT EXISTS "${PS5VK_OVERLAY_IMGUI_INCLUDE_DIR}/imgui.h")
        message(FATAL_ERROR
            "PS5VK_OVERLAY_IMGUI_INCLUDE_DIR does not contain imgui.h: ${PS5VK_OVERLAY_IMGUI_INCLUDE_DIR}")
    endif()
    target_include_directories(ps5vk_overlay PUBLIC "${PS5VK_OVERLAY_IMGUI_INCLUDE_DIR}")
else()
    message(FATAL_ERROR
        "PS5VK-Overlay needs the host project's Dear ImGui target or include directory. "
        "Set PS5VK_OVERLAY_IMGUI_TARGET or PS5VK_OVERLAY_IMGUI_INCLUDE_DIR when configuring.")
endif()

