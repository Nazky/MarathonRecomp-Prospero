#include <api/Marathon.h>
#include <os/logger.h>
#include <ui/fader.h>
#include <ui/message_window.h>
#include <ui/mods_menu.h>
#include <ui/options_menu.h>
#include <Sonicteam/SoX/Input/Manager.h>

#include <cstdint>
#include <user/achievement_manager.h>
#include <user/config.h>
#include <user/paths.h>
#include <app.h>
#include <exports.h>

constexpr auto TITLE_OPTION_OUTRO_FRAMES = (1.0 / 60.0) * 45.0;
constexpr auto TITLE_OUTRO_FRAMES = (1.0 / 60.0) * 25.0;
constexpr auto TITLE_OPTION_OUTRO_TOTAL_FRAMES = TITLE_OPTION_OUTRO_FRAMES + TITLE_OUTRO_FRAMES;

static double g_titleProceedOutroTime{};
static double g_titleExitOutroTime{};

static bool g_quitMessageOpen{};
static bool g_saveDataExists{};

static bool g_isSecretDone{};
static uint32_t g_secretFlags{};
static bool g_suppressBackUntilReleased{};

namespace
{
    class ScopedTitleTaskBackInputMask
    {
        struct SavedPadButtons
        {
            Sonicteam::SoX::Input::PadState* pad{};
            uint32_t lastButtons{};
            uint32_t invertedLastButtons{};
            uint32_t pressedButtons{};
            uint32_t releasedButtons{};
        };

        SavedPadButtons m_savedButtons;

    public:
        explicit ScopedTitleTaskBackInputMask(bool shouldMask)
        {
            if (!shouldMask || !App::s_pApp || !App::s_pApp->m_pDoc)
                return;

            auto& inputManagers = App::s_pApp->m_pDoc->m_vspInputManagers;
            if (inputManagers.empty() || !inputManagers[0])
                return;

            auto& pad = inputManagers[0]->m_PadState;
            constexpr uint32_t backMask = static_cast<uint32_t>(Sonicteam::SoX::Input::KeyState_B);
            m_savedButtons = {
                &pad,
                pad.LastButtons.get(),
                pad.InvertedLastButtons.get(),
                pad.PressedButtons.get(),
                pad.ReleasedButtons.get()
            };

            // Hide Circle/O only during TitleTask::Update. Restore the pad
            // immediately afterwards so the Mods window can still use B.
            pad.LastButtons = pad.LastButtons.get() & ~backMask;
            pad.InvertedLastButtons = pad.InvertedLastButtons.get() | backMask;
            pad.PressedButtons = pad.PressedButtons.get() & ~backMask;
            pad.ReleasedButtons = pad.ReleasedButtons.get() & ~backMask;
        }

        ~ScopedTitleTaskBackInputMask()
        {
            if (!m_savedButtons.pad)
                return;

            m_savedButtons.pad->LastButtons = m_savedButtons.lastButtons;
            m_savedButtons.pad->InvertedLastButtons = m_savedButtons.invertedLastButtons;
            m_savedButtons.pad->PressedButtons = m_savedButtons.pressedButtons;
            m_savedButtons.pad->ReleasedButtons = m_savedButtons.releasedButtons;
        }
    };
}

enum
{
    SECRET_NONE,
    SECRET_UP = 1 << 0,
    SECRET_DOWN = 1 << 1,
    SECRET_LEFT = 1 << 2,
    SECRET_RIGHT = 1 << 3,
    SECRET_ALL = SECRET_UP | SECRET_DOWN | SECRET_LEFT | SECRET_RIGHT
};

bool ProcessQuitMessage(Sonicteam::TitleTask* pTitleTask)
{
    static auto s_quitMessageResult = -1;
    static std::atomic<bool> s_faderBegun = false;

    if (!g_quitMessageOpen)
        return false;

    if ((App::s_time - g_titleExitOutroTime) < TITLE_OUTRO_FRAMES)
        return true;

    static std::array<std::string, 2> s_options = { Localise("Common_Yes"), Localise("Common_No") };

    if (MessageWindow::Open(Localise("Title_Message_Quit"), &s_quitMessageResult, s_options, 1) == MSG_CLOSED)
    {
        if (s_quitMessageResult == 0)
        {
            Fader::FadeOut(1, []() { App::Exit(); });
            s_faderBegun = true;
        }
        else
        {
            // Play Title_Open.
            pTitleTask->m_MovieWaitTime = Sonicteam::TitleTask::ms_DefaultMovieWaitTime;
            GuestToHostFunction<int>(sub_825119D8, pTitleTask, 0, 0);
        }

        g_quitMessageOpen = false;
        s_quitMessageResult = -1;
    }

    return true;
}

void TitleTask_SetDefaultOption(PPCRegister& r3, PPCRegister& r4)
{
    if (!g_saveDataExists)
        return;
    
    // Set default option to CONTINUE if save data exists.
    reinterpret_cast<Sonicteam::TitleTask*>(g_memory.Translate(r3.u32))->m_SelectedIndex = 1;
    r4.u32 = 5;
}

bool TitleTask_RedirectStateTransitionToOutroAnim(PPCRegister& r31)
{
    if (!g_saveDataExists || Config::DisableTitleInputDelay)
        return false;

    // Play Title_Close_02.
    GuestToHostFunction<int>(sub_825119D8, r31.u32, 8, 0);

    return true;
}

// Sonicteam::TitleTask::Update
PPC_FUNC_IMPL(__imp__sub_825126A0);
PPC_FUNC(sub_825126A0)
{
    auto pTitleTask = (Sonicteam::TitleTask*)(base + ctx.r3.u32);
    auto deltaTime = ctx.f1.f64;

    switch (pTitleTask->m_State)
    {
        case Sonicteam::TitleTask::TitleState_Open:
        {
            if (!Config::DisableTitleInputDelay)
                break;

            // Skip open animation.
            GuestToHostFunction<int>(sub_82511CA0, pTitleTask, (int)Sonicteam::TitleTask::TitleState_Wait);

            break;
        }

        case Sonicteam::TitleTask::TitleState_Wait:
        {
            if (g_isSecretDone)
                break;

            if (auto& spInputManager = App::s_pApp->m_pDoc->m_vspInputManagers[0])
            {
                auto& rPadState = spInputManager->m_PadState;

                if (rPadState.IsPressed(Sonicteam::SoX::Input::KeyState_DPadUp))
                    g_secretFlags = SECRET_UP;

                if ((g_secretFlags & SECRET_UP) != 0 && rPadState.IsPressed(Sonicteam::SoX::Input::KeyState_DPadDown))
                    g_secretFlags |= SECRET_DOWN;

                if ((g_secretFlags & SECRET_DOWN) != 0 && rPadState.IsPressed(Sonicteam::SoX::Input::KeyState_DPadLeft))
                    g_secretFlags |= SECRET_LEFT;

                if ((g_secretFlags & SECRET_LEFT) != 0 && rPadState.IsPressed(Sonicteam::SoX::Input::KeyState_DPadRight))
                    g_secretFlags |= SECRET_RIGHT;
            }

            if (g_secretFlags == SECRET_ALL)
            {
                Game_PlaySound("totalring_count");
                g_isSecretDone = true;
            }

            break;
        }

        case Sonicteam::TitleTask::TitleState_OptionsWait:
        {
            if (auto& spInputManager = App::s_pApp->m_pDoc->m_vspInputManagers[0])
            {
                auto& rPadState = spInputManager->m_PadState;

                if (g_secretFlags == SECRET_ALL &&
                    rPadState.IsDown(Sonicteam::SoX::Input::KeyState_A) &&
                    rPadState.IsDown(Sonicteam::SoX::Input::KeyState_Start))
                {
                    OptionsMenu::s_isDebugUnlocked = true;
                }
            }

            break;
        }

        case Sonicteam::TitleTask::TitleState_OptionsProceed:
        {
            // Reset achievements on new game.
            if (pTitleTask->m_SelectedIndex == 0)
            {
                LOGN("Resetting achievements...");

                AchievementManager::Reset();
            }

            if (!Config::DisableTitleInputDelay)
            {
                g_titleProceedOutroTime += deltaTime;

                // Wait for outro animation to complete before entering menu.
                if (g_titleProceedOutroTime > TITLE_OPTION_OUTRO_TOTAL_FRAMES)
                    GuestToHostFunction<int>(sub_82511CA0, pTitleTask, 9);
            }

            break;
        }

        case Sonicteam::TitleTask::TitleState_Proceed:
        {
            g_saveDataExists = std::filesystem::exists(GetSaveFilePath(false));

            // Redirect PRESS START proceed to options open.
            if (g_saveDataExists)
                GuestToHostFunction<int>(sub_82511CA0, pTitleTask, (int)Sonicteam::TitleTask::TitleState_OptionsOpen);

            break;
        }
    }

    if (pTitleTask->m_State != Sonicteam::TitleTask::TitleState_OptionsProceed)
        g_titleProceedOutroTime = 0.0;

    const bool modsMenuBlocksInput = ModsMenu::BlocksMainMenuInput();
    if (modsMenuBlocksInput)
    {
        g_suppressBackUntilReleased = true;
    }
    else if (g_suppressBackUntilReleased)
    {
        bool backIsStillHeld = false;
        if (App::s_pApp && App::s_pApp->m_pDoc)
        {
            for (auto& inputManager : App::s_pApp->m_pDoc->m_vspInputManagers)
            {
                if (inputManager &&
                    (inputManager->m_PadState.IsDown(Sonicteam::SoX::Input::KeyState_B) ||
                        inputManager->m_PadState.IsPressed(Sonicteam::SoX::Input::KeyState_B)))
                {
                    backIsStillHeld = true;
                    break;
                }
            }
        }
        if (!backIsStillHeld)
            g_suppressBackUntilReleased = false;
    }
    const bool blockTitleBackInput = modsMenuBlocksInput || g_suppressBackUntilReleased;
    const bool skipStateUpdate = blockTitleBackInput ? false : ProcessQuitMessage(pTitleTask);

    if (skipStateUpdate)
    {
        GuestToHostFunction<int>(sub_82511B10, pTitleTask, deltaTime);

        pTitleTask->m_MovieWaitTime = pTitleTask->m_MovieWaitTime - deltaTime;
        pTitleTask->m_Field60 = 0;

        if (pTitleTask->m_MovieWaitTime < 0.0f)
            pTitleTask->m_MovieWaitTime = 0.0f;
    }
    else
    {
        if (!blockTitleBackInput && App::s_pApp && App::s_pApp->m_pDoc &&
            !App::s_pApp->m_pDoc->m_vspInputManagers.empty() && App::s_pApp->m_pDoc->m_vspInputManagers[0])
        {
            auto& rPadState = App::s_pApp->m_pDoc->m_vspInputManagers[0]->m_PadState;

            if (rPadState.IsPressed(Sonicteam::SoX::Input::KeyState_B))
            {
                if (pTitleTask->m_State == Sonicteam::TitleTask::TitleState_Wait)
                {
                    // Play Title_Close_03.
                    Game_PlaySound("deside");
                    GuestToHostFunction<int>(sub_825119D8, pTitleTask, 9, 0);

                    g_titleExitOutroTime = App::s_time;
                    g_quitMessageOpen = true;
                }
                else if (pTitleTask->m_State == Sonicteam::TitleTask::TitleState_OptionsWait)
                {
                    // Return to title wait.
                    Game_PlaySound("window_close");
                    GuestToHostFunction<int>(sub_825119D8, pTitleTask, 1, 1);
                    GuestToHostFunction<int>(sub_82511CA0, pTitleTask, (int)Sonicteam::TitleTask::TitleState_Wait);
                }
            }
        }

        ScopedTitleTaskBackInputMask backInputMask(blockTitleBackInput);
        __imp__sub_825126A0(ctx, base);
    }
}

PPC_FUNC_IMPL(__imp__sub_82511540);
PPC_FUNC(sub_82511540)
{
    if (Config::DisableTitleInputDelay)
    {
        ctx.r3.u32 = 1;
        return;
    }

    __imp__sub_82511540(ctx, base);
}
