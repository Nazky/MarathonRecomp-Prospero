#pragma once

class ModsMenu
{
public:
    static inline bool s_isVisible{};
    static inline bool s_isRestartRequired{};

    static void Init();
    static void Draw();
    static void Open();
    static void Close();
    static bool BlocksMainMenuInput();
    static bool CommitRestartSettings();
    static void RevertRestartSettings();
};
