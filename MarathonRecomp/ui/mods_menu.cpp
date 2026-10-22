#include "mods_menu.h"

#include <app.h>
#include <exports.h>
#include <locale/locale.h>
#include <mod/mod_loader.h>
#include <Sonicteam/SoX/Input/Manager.h>
#include <patches/MainMenuTask_patches.h>
#include <patches/aspect_ratio_patches.h>
#include <ui/button_window.h>
#include <ui/fader.h>
#include <ui/imgui_utils.h>
#include <ui/message_window.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr size_t MAX_VISIBLE_MOD_ROWS = 5;
    constexpr size_t MAX_VISIBLE_OPTION_ROWS = 6;

    std::vector<UserModInfo> g_originalMods;
    std::vector<UserModInfo> g_workingMods;
    size_t g_selectedModIndex{};
    size_t g_selectedOptionIndex{};
    size_t g_firstVisibleModIndex{};
    size_t g_firstVisibleOptionIndex{};
    bool g_isEditingOptions{};
    bool g_upWasHeld{};
    bool g_downWasHeld{};
    bool g_leftWasHeld{};
    bool g_rightWasHeld{};
    bool g_isRestarting{};
    double g_openTime{};
    double g_selectionTime{};
    std::string g_errorMessage;
    int g_restartMessageResult{ -1 };

    struct MenuInput
    {
        bool up{};
        bool down{};
        bool left{};
        bool right{};
        bool accept{};
        bool back{};
        bool options{};
    };

    static bool IsHeld(const Sonicteam::SoX::Input::PadState& pad, Sonicteam::SoX::Input::KeyState key)
    {
        if (pad.IsDown(key))
            return true;
        if (key == Sonicteam::SoX::Input::KeyState_DPadUp)
            return -pad.LeftStickVertical > 0.5f;
        if (key == Sonicteam::SoX::Input::KeyState_DPadDown)
            return -pad.LeftStickVertical < -0.5f;
        if (key == Sonicteam::SoX::Input::KeyState_DPadLeft)
            return -pad.LeftStickHorizontal > 0.5f;
        if (key == Sonicteam::SoX::Input::KeyState_DPadRight)
            return -pad.LeftStickHorizontal < -0.5f;
        return false;
    }

    static MenuInput ReadMenuInput()
    {
        MenuInput input;
        bool upHeld = false;
        bool downHeld = false;
        bool leftHeld = false;
        bool rightHeld = false;

        if (App::s_pApp && App::s_pApp->m_pDoc)
        {
            for (auto& inputManager : App::s_pApp->m_pDoc->m_vspInputManagers)
            {
                if (!inputManager)
                    continue;
                const auto& pad = inputManager->m_PadState;
                upHeld = upHeld || IsHeld(pad, Sonicteam::SoX::Input::KeyState_DPadUp);
                downHeld = downHeld || IsHeld(pad, Sonicteam::SoX::Input::KeyState_DPadDown);
                leftHeld = leftHeld || IsHeld(pad, Sonicteam::SoX::Input::KeyState_DPadLeft);
                rightHeld = rightHeld || IsHeld(pad, Sonicteam::SoX::Input::KeyState_DPadRight);
                input.accept = input.accept || pad.IsPressed(Sonicteam::SoX::Input::KeyState_A);
                input.back = input.back || pad.IsPressed(Sonicteam::SoX::Input::KeyState_B);
                input.options = input.options || pad.IsPressed(Sonicteam::SoX::Input::KeyState_X);
            }
        }

        input.up = upHeld && !g_upWasHeld;
        input.down = downHeld && !g_downWasHeld;
        input.left = leftHeld && !g_leftWasHeld;
        input.right = rightHeld && !g_rightWasHeld;
        g_upWasHeld = upHeld;
        g_downWasHeld = downHeld;
        g_leftWasHeld = leftHeld;
        g_rightWasHeld = rightHeld;
        return input;
    }

    static void UpdateButtonGuide()
    {
        if (g_isEditingOptions)
            ButtonWindow::Open("Button_Mods_ChangeBack", false);
        else if (g_workingMods.empty())
            ButtonWindow::Open("Button_Mods_EmptyBack", false);
        else
            ButtonWindow::Open("Button_Mods_ToggleOptionsBack", false);
    }

    static const UserModInfo* GetSelectedMod()
    {
        return g_selectedModIndex < g_workingMods.size() ? &g_workingMods[g_selectedModIndex] : nullptr;
    }

    static std::string GetChoiceDisplayName(const ModConfigOption& option)
    {
        const auto choice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& value)
        {
            return value.value == option.value;
        });
        if (choice != option.choices.end())
        {
            std::string normalizedType = option.type;
            std::transform(normalizedType.begin(), normalizedType.end(), normalizedType.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            if ((normalizedType == "bool" || normalizedType == "boolean") &&
                (option.value == "true" || option.value == "false"))
                return Localise(option.value == "true" ? "Common_On" : "Common_Off");
            return choice->displayName;
        }
        if (option.value.empty())
            return Localise("Mods_Value_Default");
        return Localise("Mods_Value_Custom") + ": " + option.value;
    }

    static std::string GetSelectedOptionDescription(const ModConfigOption& option)
    {
        const auto choice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& value)
        {
            return value.value == option.value;
        });
        if (choice != option.choices.end() && !choice->description.empty())
            return choice->description;
        return option.description;
    }

    static void CycleSelectedOption(bool backwards)
    {
        auto* mod = g_selectedModIndex < g_workingMods.size() ? &g_workingMods[g_selectedModIndex] : nullptr;
        if (!mod || g_selectedOptionIndex >= mod->options.size())
            return;
        auto& option = mod->options[g_selectedOptionIndex];
        if (option.choices.empty())
            return;

        const auto currentChoice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& choice)
        {
            return choice.value == option.value;
        });
        const size_t currentIndex = currentChoice == option.choices.end()
            ? 0
            : static_cast<size_t>(std::distance(option.choices.begin(), currentChoice));
        const size_t nextIndex = currentChoice == option.choices.end()
            ? 0
            : backwards
                ? (currentIndex + option.choices.size() - 1) % option.choices.size()
                : (currentIndex + 1) % option.choices.size();
        option.value = option.choices[nextIndex].value;
        g_errorMessage.clear();
        g_selectionTime = ImGui::GetTime();
        Game_PlaySound("main_deside");
    }

    static void ProcessMenuInput()
    {
        const MenuInput input = ReadMenuInput();
        if (g_isEditingOptions)
        {
            const auto* mod = GetSelectedMod();
            if (input.back)
            {
                g_isEditingOptions = false;
                g_selectedOptionIndex = 0;
                g_firstVisibleOptionIndex = 0;
                g_errorMessage.clear();
                UpdateButtonGuide();
                Game_PlaySound("window_close");
                return;
            }
            if (!mod || mod->options.empty())
                return;

            if (input.up)
            {
                g_selectedOptionIndex = g_selectedOptionIndex == 0 ? mod->options.size() - 1 : g_selectedOptionIndex - 1;
                g_selectionTime = ImGui::GetTime();
                Game_PlaySound("move");
            }
            else if (input.down)
            {
                g_selectedOptionIndex = (g_selectedOptionIndex + 1) % mod->options.size();
                g_selectionTime = ImGui::GetTime();
                Game_PlaySound("move");
            }
            else if (input.options || input.right)
                CycleSelectedOption(false);
            else if (input.left)
                CycleSelectedOption(true);
            return;
        }

        if (input.back)
        {
            Game_PlaySound("window_close");
            ModsMenu::Close();
            return;
        }

        if (g_workingMods.empty())
            return;

        if (input.up)
        {
            g_selectedModIndex = g_selectedModIndex == 0 ? g_workingMods.size() - 1 : g_selectedModIndex - 1;
            g_selectionTime = ImGui::GetTime();
            Game_PlaySound("move");
        }
        else if (input.down)
        {
            g_selectedModIndex = (g_selectedModIndex + 1) % g_workingMods.size();
            g_selectionTime = ImGui::GetTime();
            Game_PlaySound("move");
        }
        else if (input.accept)
        {
            g_workingMods[g_selectedModIndex].enabled = !g_workingMods[g_selectedModIndex].enabled;
            g_errorMessage.clear();
            g_selectionTime = ImGui::GetTime();
            Game_PlaySound("main_deside");
        }
        else if (input.left || input.right)
        {
            const int direction = input.left ? -1 : 1;
            const auto nextIndex = static_cast<std::ptrdiff_t>(g_selectedModIndex) + direction;
            if (nextIndex >= 0 && nextIndex < static_cast<std::ptrdiff_t>(g_workingMods.size()))
            {
                std::swap(g_workingMods[g_selectedModIndex], g_workingMods[static_cast<size_t>(nextIndex)]);
                g_selectedModIndex = static_cast<size_t>(nextIndex);
                g_errorMessage.clear();
                g_selectionTime = ImGui::GetTime();
                Game_PlaySound("move");
            }
        }
        else if (input.options)
        {
            const auto* mod = GetSelectedMod();
            if (mod && !mod->options.empty())
            {
                g_isEditingOptions = true;
                g_selectedOptionIndex = 0;
                g_firstVisibleOptionIndex = 0;
                g_errorMessage.clear();
                UpdateButtonGuide();
                Game_PlaySound("main_deside");
            }
        }
    }

    static bool HasChanges()
    {
        if (g_workingMods.size() != g_originalMods.size())
            return true;
        for (size_t i = 0; i < g_workingMods.size(); ++i)
        {
            const auto& workingMod = g_workingMods[i];
            const auto originalMod = std::find_if(g_originalMods.begin(), g_originalMods.end(), [&](const UserModInfo& mod)
            {
                return mod.folderName == workingMod.folderName;
            });
            if (originalMod == g_originalMods.end() || workingMod.folderName != g_originalMods[i].folderName ||
                workingMod.enabled != originalMod->enabled || workingMod.options.size() != originalMod->options.size())
                return true;
            for (const auto& workingOption : workingMod.options)
            {
                const auto originalOption = std::find_if(originalMod->options.begin(), originalMod->options.end(), [&](const ModConfigOption& option)
                {
                    return option.sectionName == workingOption.sectionName && option.name == workingOption.name && option.iniFile == workingOption.iniFile;
                });
                if (originalOption == originalMod->options.end() || originalOption->value != workingOption.value)
                    return true;
            }
        }
        return false;
    }

    static void UpdateVisibleStart(size_t selectedIndex, size_t& firstVisibleIndex, size_t visibleCount)
    {
        if (visibleCount == 0)
            return;
        if (selectedIndex < firstVisibleIndex)
            firstVisibleIndex = selectedIndex;
        else if (selectedIndex >= firstVisibleIndex + visibleCount)
            firstVisibleIndex = selectedIndex - visibleCount + 1;
    }

    static void DrawWrappedText(ImDrawList* drawList, const std::string& text, ImVec2 position, float maxWidth, float fontSize, ImU32 colour)
    {
        DrawTextParagraph(g_pFntRodin, fontSize, maxWidth, position, Scale(2, true), text.c_str(),
            [drawList, fontSize, colour](const char* line, ImVec2 linePosition)
            {
                drawList->AddText(g_pFntRodin, fontSize, linePosition, colour, line);
            });
    }

    static void DrawModList(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha)
    {
        const auto& screen = ImGui::GetIO().DisplaySize;
        (void)screen;
        const float rowHeight = Scale(72, true);
        const float rowGap = Scale(7, true);
        const float availableHeight = std::max(0.0f, max.y - min.y);
        const size_t visibleCount = std::clamp<size_t>(
            static_cast<size_t>(availableHeight / (rowHeight + rowGap)), 1, MAX_VISIBLE_MOD_ROWS);
        UpdateVisibleStart(g_selectedModIndex, g_firstVisibleModIndex, visibleCount);
        if (g_workingMods.empty())
            return;
        const size_t endIndex = std::min(g_workingMods.size(), g_firstVisibleModIndex + visibleCount);
        for (size_t i = g_firstVisibleModIndex; i < endIndex; ++i)
        {
            const auto& mod = g_workingMods[i];
            const bool selected = i == g_selectedModIndex;
            const float rowY = min.y + static_cast<float>(i - g_firstVisibleModIndex) * (rowHeight + rowGap);
            const ImVec2 rowMin{ min.x, rowY };
            const ImVec2 rowMax{ max.x, rowY + rowHeight };
            const ImU32 fill = selected
                ? IM_COL32(31, 88, 143, static_cast<int>(220 * alpha))
                : IM_COL32(4, 12, 24, static_cast<int>(150 * alpha));
            const ImU32 outline = selected
                ? IM_COL32(210, 235, 255, static_cast<int>(190 * alpha))
                : IM_COL32(160, 190, 220, static_cast<int>(55 * alpha));
            drawList->AddRectFilled(rowMin, rowMax, fill, Scale(4, true));
            drawList->AddRect(rowMin, rowMax, outline, Scale(4, true));
            if (selected)
                DrawArrowCursor({ rowMin.x + Scale(4, true), rowMin.y + Scale(20, true) }, g_selectionTime, false, false);

            const std::string title = mod.title.empty() ? mod.folderName : mod.title;
            const float textX = rowMin.x + Scale(38, true);
            const float titleY = rowMin.y + Scale(10, true);
            const auto status = Localise(mod.enabled ? "Options_Mods_Enabled" : "Options_Mods_Disabled");
            const float smallSize = g_fntRodinSize * 0.72f;
            const auto statusSize = g_pFntRodin->CalcTextSizeA(smallSize, FLT_MAX, 0.0f, status.c_str());
            const ImVec2 statusPosition{ rowMax.x - statusSize.x - Scale(10, true), rowMin.y + Scale(12, true) };
            drawList->PushClipRect({ textX, rowMin.y }, { statusPosition.x - Scale(8, true), rowMax.y }, true);
            drawList->AddText(g_pFntRodin, g_fntRodinSize, { textX, titleY }, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), title.c_str());
            drawList->PopClipRect();
            drawList->AddText(g_pFntRodin, smallSize, statusPosition, mod.enabled
                ? IM_COL32(165, 235, 180, static_cast<int>(245 * alpha))
                : IM_COL32(220, 190, 170, static_cast<int>(225 * alpha)), status.c_str());

            std::string meta = mod.folderName + "  |  " + std::to_string(mod.options.size()) + " " + Localise("Mods_OptionsCount");
            drawList->PushClipRect({ textX, rowMin.y + Scale(38, true) }, { rowMax.x - Scale(10, true), rowMax.y }, true);
            drawList->AddText(g_pFntRodin, smallSize, { textX, rowMin.y + Scale(41, true) }, IM_COL32(205, 220, 235, static_cast<int>(220 * alpha)), meta.c_str());
            drawList->PopClipRect();
        }
    }

    static void DrawSelectedModDetails(ImDrawList* drawList, const UserModInfo& mod, ImVec2 min, ImVec2 max, float alpha)
    {
        const float fontSize = g_fntRodinSize;
        const float smallSize = g_fntRodinSize * 0.78f;
        const auto title = mod.title.empty() ? mod.folderName : mod.title;
        drawList->AddText(g_pFntRodin, Scale(25, true), min, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), title.c_str());

        float y = min.y + Scale(43, true);
        const std::string folderLine = Localise("Mods_Folder") + ": " + mod.folderName;
        drawList->AddText(g_pFntRodin, smallSize, { min.x, y }, IM_COL32(185, 210, 230, static_cast<int>(220 * alpha)), folderLine.c_str());
        y += Scale(27, true);

        if (!mod.author.empty())
        {
            const std::string authorLine = Localise("Mods_Author") + ": " + mod.author;
            drawList->AddText(g_pFntRodin, smallSize, { min.x, y }, IM_COL32(210, 220, 230, static_cast<int>(235 * alpha)), authorLine.c_str());
            y += Scale(30, true);
        }
        else
            y += Scale(8, true);

        std::string description = mod.description.empty() ? Localise("Mods_NoDescription") : mod.description;
        const float descriptionHeight = Scale(120, true);
        drawList->PushClipRect({ min.x, y }, { max.x, std::min(max.y, y + descriptionHeight) }, true);
        DrawWrappedText(drawList, description, { min.x, y }, max.x - min.x, fontSize * 0.9f,
            IM_COL32(225, 232, 238, static_cast<int>(235 * alpha)));
        drawList->PopClipRect();
        y += descriptionHeight + Scale(16, true);

        const std::string optionCount = Localise("Mods_OptionCount") + ": " + std::to_string(mod.options.size());
        drawList->AddText(g_pFntRodin, smallSize, { min.x, y }, IM_COL32(195, 220, 205, static_cast<int>(245 * alpha)), optionCount.c_str());
        y += Scale(34, true);

        if (mod.options.empty())
        {
            const auto noOptions = Localise("Mods_NoOptions");
            DrawWrappedText(drawList, noOptions, { min.x, y }, max.x - min.x, smallSize,
                IM_COL32(185, 198, 210, static_cast<int>(220 * alpha)));
        }
        else
        {
            const auto help = Localise("Mods_Help_Order");
            DrawWrappedText(drawList, help, { min.x, y }, max.x - min.x, smallSize,
                IM_COL32(185, 198, 210, static_cast<int>(220 * alpha)));
        }
    }

    static void DrawOptionsList(ImDrawList* drawList, const UserModInfo& mod, ImVec2 min, ImVec2 max, float alpha)
    {
        if (mod.options.empty())
        {
            DrawWrappedText(drawList, Localise("Mods_NoOptions"), min, max.x - min.x, g_fntRodinSize,
                IM_COL32(205, 215, 225, static_cast<int>(235 * alpha)));
            return;
        }

        const float rowHeight = Scale(58, true);
        const float rowGap = Scale(6, true);
        const float availableHeight = std::max(0.0f, max.y - min.y - Scale(118, true));
        const size_t visibleCount = std::clamp<size_t>(
            static_cast<size_t>(availableHeight / (rowHeight + rowGap)), 1, MAX_VISIBLE_OPTION_ROWS);
        UpdateVisibleStart(g_selectedOptionIndex, g_firstVisibleOptionIndex, visibleCount);
        const size_t endIndex = std::min(mod.options.size(), g_firstVisibleOptionIndex + visibleCount);

        for (size_t i = g_firstVisibleOptionIndex; i < endIndex; ++i)
        {
            const auto& option = mod.options[i];
            const bool selected = i == g_selectedOptionIndex;
            const float rowY = min.y + static_cast<float>(i - g_firstVisibleOptionIndex) * (rowHeight + rowGap);
            const ImVec2 rowMin{ min.x, rowY };
            const ImVec2 rowMax{ max.x, rowY + rowHeight };
            drawList->AddRectFilled(rowMin, rowMax,
                selected ? IM_COL32(30, 87, 133, static_cast<int>(205 * alpha)) : IM_COL32(4, 12, 24, static_cast<int>(130 * alpha)), Scale(4, true));
            drawList->AddRect(rowMin, rowMax,
                selected ? IM_COL32(210, 235, 255, static_cast<int>(180 * alpha)) : IM_COL32(160, 190, 220, static_cast<int>(45 * alpha)), Scale(4, true));
            if (selected)
                DrawArrowCursor({ rowMin.x + Scale(4, true), rowMin.y + Scale(12, true) }, g_selectionTime, false, false);

            const float textX = rowMin.x + Scale(34, true);
            const float valueSize = g_fntRodinSize * 0.8f;
            const std::string value = option.choices.empty() ? Localise("Mods_ReadOnly") : GetChoiceDisplayName(option);
            const auto valueSizePx = g_pFntRodin->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, value.c_str());
            const ImVec2 valuePosition{ rowMax.x - valueSizePx.x - Scale(12, true), rowMin.y + Scale(16, true) };
            const float groupSize = g_fntRodinSize * 0.58f;
            const std::string displayName = option.displayName.empty() ? option.name : option.displayName;
            drawList->PushClipRect({ textX, rowMin.y }, { valuePosition.x - Scale(9, true), rowMax.y }, true);
            drawList->AddText(g_pFntRodin, groupSize, { textX, rowMin.y + Scale(4, true) }, IM_COL32(175, 205, 230, static_cast<int>(220 * alpha)), option.groupDisplayName.c_str());
            drawList->AddText(g_pFntRodin, valueSize, { textX, rowMin.y + Scale(24, true) }, IM_COL32(255, 255, 255, static_cast<int>(245 * alpha)), displayName.c_str());
            drawList->PopClipRect();
            drawList->AddText(g_pFntRodin, valueSize, valuePosition, IM_COL32(196, 224, 203, static_cast<int>(245 * alpha)), value.c_str());
        }

        if (g_selectedOptionIndex < mod.options.size())
        {
            const auto& option = mod.options[g_selectedOptionIndex];
            std::string description = GetSelectedOptionDescription(option);
            if (description.empty())
                description = option.groupDisplayName;
            const float descY = max.y - Scale(101, true);
            drawList->AddLine({ min.x, descY - Scale(10, true) }, { max.x, descY - Scale(10, true) }, IM_COL32(175, 202, 226, static_cast<int>(100 * alpha)), Scale(1, true));
            DrawWrappedText(drawList, description, { min.x, descY }, max.x - min.x, g_fntRodinSize * 0.78f,
                IM_COL32(215, 225, 232, static_cast<int>(235 * alpha)));
        }
    }

    static void DrawMenu()
    {
        auto* drawList = ImGui::GetBackgroundDrawList();
        const auto screen = ImGui::GetIO().DisplaySize;
        const float alpha = static_cast<float>(ComputeLinearMotion(g_openTime, 0, 7));
        drawList->AddRectFilled({ 0, 0 }, screen, IM_COL32(0, 0, 0, static_cast<int>(155 * alpha)));

        const float marginX = Scale(72, true);
        const float marginY = Scale(38, true);
        const ImVec2 panelMin{ g_horzCentre + marginX, g_vertCentre + marginY };
        const ImVec2 panelMax{ screen.x - g_horzCentre - marginX, screen.y - g_vertCentre - marginY };
        const double windowMotion = DrawWindow(panelMin, panelMax, true, g_openTime);
        const float contentAlpha = static_cast<float>(windowMotion) * alpha;
        const float panelWidth = panelMax.x - panelMin.x;
        const float panelHeight = panelMax.y - panelMin.y;
        const float insetX = Scale(34, true);
        const float headerY = panelMin.y + Scale(24, true);
        const float contentTop = panelMin.y + Scale(98, true);
        const float footerTop = panelMax.y - Scale(46, true);
        const float listWidth = panelWidth * 0.44f;
        const ImVec2 listMin{ panelMin.x + insetX, contentTop };
        const ImVec2 listMax{ panelMin.x + listWidth, footerTop };
        const ImVec2 detailMin{ listMax.x + Scale(28, true), contentTop };
        const ImVec2 detailMax{ panelMax.x - insetX, footerTop };

        if (App::s_isInit)
            SetShaderModifier(IMGUI_SHADER_MODIFIER_LOW_QUALITY_TEXT);

        drawList->AddText(g_pFntNewRodin, Scale(31, true), { panelMin.x + insetX, headerY },
            IM_COL32(255, 255, 255, static_cast<int>(255 * contentAlpha)), Localise("Mods_Title").c_str());
        const std::string headerHelp = Localise(g_isEditingOptions ? "Mods_Help_Options" : "Mods_Help_List");
        drawList->AddText(g_pFntRodin, g_fntRodinSize * 0.75f, { panelMin.x + insetX, headerY + Scale(43, true) },
            IM_COL32(190, 213, 232, static_cast<int>(235 * contentAlpha)), headerHelp.c_str());

        drawList->AddLine({ listMax.x + Scale(13, true), contentTop }, { listMax.x + Scale(13, true), footerTop },
            IM_COL32(190, 215, 240, static_cast<int>(95 * contentAlpha)), Scale(1, true));

        if (g_workingMods.empty())
        {
            const std::string emptyText = Localise("Mods_Empty");
            const std::string folderText = ModLoader::GetManagedModsDirectory().string();
            DrawWrappedText(drawList, emptyText, listMin, listMax.x - listMin.x, g_fntRodinSize,
                IM_COL32(235, 240, 245, static_cast<int>(250 * contentAlpha)));
            DrawWrappedText(drawList, Localise("Mods_EmptyHint"), { listMin.x, listMin.y + Scale(58, true) }, listMax.x - listMin.x,
                g_fntRodinSize * 0.8f, IM_COL32(190, 210, 225, static_cast<int>(230 * contentAlpha)));
            DrawWrappedText(drawList, folderText, { listMin.x, listMin.y + Scale(126, true) }, listMax.x - listMin.x,
                g_fntRodinSize * 0.72f, IM_COL32(165, 205, 178, static_cast<int>(230 * contentAlpha)));
        }
        else
        {
            DrawModList(drawList, listMin, listMax, contentAlpha);
            if (const auto* mod = GetSelectedMod())
            {
                if (g_isEditingOptions)
                    DrawOptionsList(drawList, *mod, detailMin, detailMax, contentAlpha);
                else
                    DrawSelectedModDetails(drawList, *mod, detailMin, detailMax, contentAlpha);
            }
        }

        if (!g_errorMessage.empty())
        {
            const float errorY = footerTop + Scale(9, true);
            drawList->AddText(g_pFntRodin, g_fntRodinSize * 0.72f, { listMin.x, errorY },
                IM_COL32(255, 140, 135, static_cast<int>(255 * contentAlpha)), g_errorMessage.c_str());
        }

        const std::string countText = std::to_string(g_workingMods.size()) + " " + Localise("Mods_Installed");
        drawList->AddText(g_pFntRodin, g_fntRodinSize * 0.7f, { detailMin.x, panelMax.y - Scale(31, true) },
            IM_COL32(170, 196, 215, static_cast<int>(205 * contentAlpha)), countText.c_str());

        if (App::s_isInit)
            SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);
        drawList->PopClipRect();
        (void)panelHeight;
    }

    static void DrawRestartPrompt()
    {
        std::array<std::string, 2> buttons = { Localise("Common_Yes"), Localise("Common_No") };
        std::string message = Localise("Mods_Message_RestartConfirmClose");
        message += "\n- ";
        message += Localise("Mods_RestartChange");
        if (MessageWindow::Open(std::move(message), &g_restartMessageResult, buttons) != MSG_CLOSED)
            return;

        const int result = g_restartMessageResult;
        g_restartMessageResult = -1;
        if (result == 0)
        {
            if (ModsMenu::CommitRestartSettings())
            {
                g_isRestarting = true;
                Fader::FadeOut(1, []()
                {
                    App::RestartForSettings({ "--use-cwd --skip-logos" });
                });
            }
        }
        else
            ModsMenu::RevertRestartSettings();
    }
}

void ModsMenu::Init()
{
}

void ModsMenu::Draw()
{
    if (s_isVisible)
    {
        ProcessMenuInput();
        if (s_isVisible)
            DrawMenu();
    }
    else if (s_isRestartRequired)
        DrawRestartPrompt();
}

void ModsMenu::Open()
{
    if (s_isVisible)
        return;

    g_originalMods = ModLoader::GetUserMods();
    g_workingMods = g_originalMods;
    g_selectedModIndex = 0;
    g_selectedOptionIndex = 0;
    g_firstVisibleModIndex = 0;
    g_firstVisibleOptionIndex = 0;
    g_isEditingOptions = false;
    g_upWasHeld = false;
    g_downWasHeld = false;
    g_leftWasHeld = false;
    g_rightWasHeld = false;
    g_openTime = ImGui::GetTime();
    g_selectionTime = g_openTime;
    g_errorMessage.clear();
    g_restartMessageResult = -1;
    g_isRestarting = false;
    s_isRestartRequired = false;
    s_isVisible = true;
    MainMenuTaskPatches::s_hideButtonWindow = true;
    UpdateButtonGuide();
}

void ModsMenu::Close()
{
    if (!s_isVisible)
        return;

    s_isVisible = false;
    g_isEditingOptions = false;
    s_isRestartRequired = HasChanges();
    if (!s_isRestartRequired)
        g_workingMods = g_originalMods;
    ButtonWindow::Close();
    if (!s_isRestartRequired)
        MainMenuTaskPatches::s_hideButtonWindow = false;
}

bool ModsMenu::BlocksMainMenuInput()
{
    return s_isVisible || s_isRestartRequired || g_isRestarting;
}

bool ModsMenu::CommitRestartSettings()
{
    std::string errorMessage;
    if (!ModLoader::CommitUserMods(g_workingMods, errorMessage))
    {
        g_errorMessage = errorMessage.empty() ? Localise("Mods_Error_SaveFailed") : std::move(errorMessage);
        s_isRestartRequired = false;
        s_isVisible = true;
        g_isEditingOptions = false;
        MainMenuTaskPatches::s_hideButtonWindow = true;
        UpdateButtonGuide();
        return false;
    }

    g_originalMods = g_workingMods;
    s_isRestartRequired = false;
    g_errorMessage.clear();
    return true;
}

void ModsMenu::RevertRestartSettings()
{
    g_workingMods = g_originalMods;
    s_isRestartRequired = false;
    g_isEditingOptions = false;
    g_errorMessage.clear();
    MainMenuTaskPatches::s_hideButtonWindow = false;
}
