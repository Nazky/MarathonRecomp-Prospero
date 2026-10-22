#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct ModConfigChoice
{
    std::string displayName;
    std::string value;
    std::string description;
};

struct ModConfigOption
{
    std::string sectionName;
    std::string groupDisplayName;
    std::string name;
    std::string displayName;
    std::string description;
    std::string type;
    std::string iniFile;
    std::string defaultValue;
    std::string value;
    std::vector<ModConfigChoice> choices;
};

struct UserModInfo
{
    // Folder name beneath the managed mods directory (also the persistent ID in mods.json).
    std::string folderName;
    std::string title;
    std::string description;
    std::string author;
    bool enabled = true;
    std::vector<ModConfigOption> options;
};

struct ModLoader
{
    static inline bool s_isLogTypeConsole;

    static inline std::filesystem::path s_saveFilePath;

    static std::filesystem::path ResolvePath(std::string_view path);

    static std::vector<std::filesystem::path>* GetIncludeDirectories(size_t modIndex);

    static std::filesystem::path GetManagedModsDirectory();
    static const std::vector<UserModInfo>& GetUserMods();
    static bool SetUserModEnabled(size_t modIndex, bool enabled, std::string& errorMessage);
    static bool MoveUserMod(size_t modIndex, int direction, std::string& errorMessage);
    static bool CommitUserMods(const std::vector<UserModInfo>& mods, std::string& errorMessage);
    static bool IsRestartRequired();

    static void Init();
};
