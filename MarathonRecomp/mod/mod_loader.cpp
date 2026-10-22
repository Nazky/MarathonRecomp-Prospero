#include "mod_loader.h"
#include "custom_filesystem.h"
#include "ini_file.h"

#include <cpu/guest_stack_var.h>
#include <kernel/function.h>
#include <kernel/heap.h>
#include <user/config.h>
#include <user/paths.h>
#include <os/logger.h>
#include <os/process.h>
#include <xxHashMap.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

enum class ModType
{
    HMM,
    UMM
};

struct Mod
{
    ModType type{};
    std::vector<std::filesystem::path> includeDirs;
    bool merge = false;
    bool customFilesystem = false;
    ankerl::unordered_dense::set<std::filesystem::path> readOnly;
};

using ModManifestJson = nlohmann::json;

static std::vector<Mod> g_mods;
static std::vector<UserModInfo> g_userMods;
static std::vector<UserModInfo> g_initialUserMods;
static std::filesystem::path g_managedModsDirectory;
static bool g_hasManagedSaveFilePath{};
static ModManifestJson g_managedManifestTemplate = ModManifestJson::object();
static std::unordered_map<std::string, ModManifestJson> g_managedModManifestTemplates;

struct ManagedModCandidate
{
    UserModInfo info;
    Mod mod;
    std::filesystem::path saveFilePath;
};

struct CustomArchiveCandidate
{
    std::string platform;
    std::string category;
    std::string name;
    std::string packagePath;
};

static std::mutex g_customFilesystemMutex;
static bool g_customFilesystemPrepared{};
static std::unordered_map<std::string, std::filesystem::path> g_customSystemArchiveOverrides;

static constexpr size_t MAX_MANAGED_MOD_COUNT = 4096;

static std::filesystem::path ManagedModsDirectory()
{
#if defined(__PROSPERO__)
    return "/data/MarathonRecomp/mods";
#else
    return GetUserPath() / "mods";
#endif
}

static std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

static std::filesystem::path PathFromUtf8(std::string_view value)
{
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

static bool IsSafeRelativeModPath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;

    for (const auto& component : path)
    {
        if (component == "..")
            return false;
    }

    return true;
}

static bool HasPathPrefix(const std::filesystem::path& parent, const std::filesystem::path& child)
{
    auto parentIt = parent.begin();
    auto childIt = child.begin();
    for (; parentIt != parent.end(); ++parentIt, ++childIt)
    {
        if (childIt == child.end() || *parentIt != *childIt)
            return false;
    }
    return true;
}

static bool IsSafeManagedModPath(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const auto normalizedRoot = root.lexically_normal();
    const auto normalizedCandidate = candidate.lexically_normal();
    if (!HasPathPrefix(normalizedRoot, normalizedCandidate))
        return false;

#if defined(__PROSPERO__)
    // Keep /data access lexical on Prospero. Metadata lookups can report EPERM
    // for valid app-data paths; ignore those while rejecting known symlinks.
    auto currentPath = normalizedRoot;
    const auto relativePath = normalizedCandidate.lexically_relative(normalizedRoot);
    for (const auto& component : relativePath)
    {
        if (component == ".")
            continue;
        if (component == "..")
            return false;

        currentPath /= component;
        std::error_code error;
        const auto status = std::filesystem::symlink_status(currentPath, error);
        if (!error && std::filesystem::is_symlink(status))
            return false;
        if (error == std::errc::no_such_file_or_directory)
            break;
        if (error == std::errc::operation_not_permitted || error == std::errc::permission_denied)
            continue;
        if (error)
            return false;
    }
    return true;
#else
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(normalizedRoot, error);
    if (error)
        return false;
    error.clear();
    const auto canonicalCandidate = std::filesystem::weakly_canonical(normalizedCandidate, error);
    return !error && HasPathPrefix(canonicalRoot, canonicalCandidate);
#endif
}

static std::string TrimAscii(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return std::string(value);
}

static bool IsSafeIniId(std::string_view value)
{
    if (value.empty() || TrimAscii(value) != value || value.front() == '[' || value.front() == ';')
        return false;
    for (const char character : value)
    {
        if (character == '=' || character == 13 || character == 10 || character == '"')
            return false;
    }
    return true;
}

static bool IsSafeIniValue(std::string_view value)
{
    for (const char character : value)
    {
        if (character == 13 || character == 10 || character == '"')
            return false;
    }
    return true;
}

static bool ReadStreamLimited(std::istream& input, std::string& contents, size_t maximumSize)
{
    contents.clear();
    char buffer[4096];
    while (input)
    {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        if (count > 0)
        {
            if (contents.size() + static_cast<size_t>(count) > maximumSize)
                return false;
            contents.append(buffer, static_cast<size_t>(count));
        }
    }
    return input.eof();
}

static std::string JsonValueToText(const nlohmann::json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_boolean())
        return value.get<bool>() ? "true" : "false";
    if (value.is_number())
        return value.dump();
    if (value.is_array())
    {
        std::string text;
        for (const auto& line : value)
        {
            if (!text.empty())
                text.push_back(static_cast<char>(10));
            if (line.is_string())
                text += line.get<std::string>();
            else if (!line.is_null())
                text += line.dump();
        }
        return text;
    }
    return {};
}

static std::string GetJsonText(const nlohmann::json& object, std::string_view key, std::string defaultValue = {})
{
    if (!object.is_object())
        return defaultValue;
    const auto it = object.find(std::string(key));
    return it == object.end() ? defaultValue : JsonValueToText(*it);
}

static std::string RemoveJsonCommentsAndTrailingCommas(std::string_view input)
{
    std::string uncommented;
    uncommented.reserve(input.size());
    bool inString = false;
    bool escaped = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (size_t i = 0; i < input.size(); ++i)
    {
        const char character = input[i];
        const char next = i + 1 < input.size() ? input[i + 1] : static_cast<char>(0);
        if (inString)
        {
            uncommented.push_back(character);
            if (escaped)
                escaped = false;
            else if (character == static_cast<char>(92))
                escaped = true;
            else if (character == '"')
                inString = false;
            continue;
        }
        if (inLineComment)
        {
            if (character == 10 || character == 13)
            {
                uncommented.push_back(character);
                inLineComment = false;
            }
            else
                uncommented.push_back(' ');
            continue;
        }
        if (inBlockComment)
        {
            if (character == '*' && next == '/')
            {
                uncommented.append("  ");
                ++i;
                inBlockComment = false;
            }
            else
                uncommented.push_back(character == 10 || character == 13 ? character : ' ');
            continue;
        }
        if (character == '"')
        {
            inString = true;
            uncommented.push_back(character);
        }
        else if (character == '/' && next == '/')
        {
            uncommented.append("  ");
            ++i;
            inLineComment = true;
        }
        else if (character == '/' && next == '*')
        {
            uncommented.append("  ");
            ++i;
            inBlockComment = true;
        }
        else
            uncommented.push_back(character);
    }

    if (uncommented.size() >= 3 &&
        static_cast<unsigned char>(uncommented[0]) == 0xEF &&
        static_cast<unsigned char>(uncommented[1]) == 0xBB &&
        static_cast<unsigned char>(uncommented[2]) == 0xBF)
        uncommented.erase(0, 3);

    std::string result;
    result.reserve(uncommented.size());
    inString = false;
    escaped = false;
    for (size_t i = 0; i < uncommented.size(); ++i)
    {
        const char character = uncommented[i];
        if (inString)
        {
            result.push_back(character);
            if (escaped)
                escaped = false;
            else if (character == static_cast<char>(92))
                escaped = true;
            else if (character == '"')
                inString = false;
            continue;
        }
        if (character == '"')
        {
            inString = true;
            result.push_back(character);
        }
        else if (character == ',')
        {
            size_t next = i + 1;
            while (next < uncommented.size() && std::isspace(static_cast<unsigned char>(uncommented[next])))
                ++next;
            if (next >= uncommented.size() || (uncommented[next] != ']' && uncommented[next] != '}'))
                result.push_back(character);
        }
        else
            result.push_back(character);
    }
    return result;
}

static void ReadManagedModOptions(const std::filesystem::path& modDirectory, const IniFile& modIni, UserModInfo& info)
{
    const std::string schemaPathText = modIni.getString("Main", "ConfigSchemaFile", std::string());
    if (schemaPathText.empty())
        return;

    std::string normalizedSchemaPath = schemaPathText;
    std::replace(normalizedSchemaPath.begin(), normalizedSchemaPath.end(), '\\', '/');
    const auto schemaRelativePath = PathFromUtf8(normalizedSchemaPath);
    if (!IsSafeRelativeModPath(schemaRelativePath))
        return;
    const auto schemaPath = (modDirectory / schemaRelativePath).lexically_normal();
    if (!IsSafeManagedModPath(modDirectory, schemaPath))
        return;

    std::ifstream schemaInput(schemaPath, std::ios::binary);
    if (!schemaInput)
        return;
    std::string schemaContents;
    if (!ReadStreamLimited(schemaInput, schemaContents, 2 * 1024 * 1024))
        return;

    const auto schema = nlohmann::json::parse(RemoveJsonCommentsAndTrailingCommas(schemaContents), nullptr, false);
    if (schema.is_discarded() || !schema.is_object())
        return;

    std::string iniFileText = GetJsonText(schema, "IniFile", "mod.ini");
    if (iniFileText.empty())
        iniFileText = "mod.ini";
    std::replace(iniFileText.begin(), iniFileText.end(), '\\', '/');
    const auto iniRelativePath = PathFromUtf8(iniFileText);
    if (!IsSafeRelativeModPath(iniRelativePath))
        return;
    const auto optionIniPath = (modDirectory / iniRelativePath).lexically_normal();
    if (!IsSafeManagedModPath(modDirectory, optionIniPath))
        return;

    IniFile optionIni;
    optionIni.read(optionIniPath);
    const auto groupsIt = schema.find("Groups");
    const auto enumsIt = schema.find("Enums");
    if (groupsIt == schema.end() || !groupsIt->is_array())
        return;

    constexpr size_t MAX_OPTIONS_PER_MOD = 512;
    for (const auto& group : *groupsIt)
    {
        if (!group.is_object())
            continue;
        const std::string groupName = GetJsonText(group, "Name");
        const std::string groupDisplayName = GetJsonText(group, "DisplayName", groupName);
        const auto elementsIt = group.find("Elements");
        if (groupName.empty() || elementsIt == group.end() || !elementsIt->is_array())
            continue;

        for (const auto& element : *elementsIt)
        {
            if (!element.is_object() || info.options.size() >= MAX_OPTIONS_PER_MOD)
                continue;
            ModConfigOption option;
            option.sectionName = groupName;
            option.groupDisplayName = groupDisplayName;
            option.name = GetJsonText(element, "Name");
            option.displayName = GetJsonText(element, "DisplayName", option.name);
            option.description = GetJsonText(element, "Description");
            option.type = GetJsonText(element, "Type");
            option.iniFile = iniFileText;
            option.defaultValue = GetJsonText(element, "DefaultValue");
            if (option.name.empty() || !IsSafeIniId(option.name) || !IsSafeIniId(option.sectionName))
                continue;

            option.value = optionIni.getString(option.sectionName, option.name, option.defaultValue);
            if (enumsIt != schema.end() && enumsIt->is_object())
            {
                const auto enumIt = enumsIt->find(option.type);
                if (enumIt != enumsIt->end() && enumIt->is_array())
                {
                    for (const auto& choiceJson : *enumIt)
                    {
                        if (!choiceJson.is_object())
                            continue;
                        ModConfigChoice choice;
                        choice.displayName = GetJsonText(choiceJson, "DisplayName");
                        choice.value = GetJsonText(choiceJson, "Value");
                        choice.description = GetJsonText(choiceJson, "Description");
                        if (choice.displayName.empty() || !IsSafeIniValue(choice.value))
                            continue;
                        option.choices.emplace_back(std::move(choice));
                    }
                }
            }

            if (option.choices.empty())
            {
                std::string normalizedType = option.type;
                std::transform(normalizedType.begin(), normalizedType.end(), normalizedType.begin(), [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
                if (normalizedType == "bool" || normalizedType == "boolean")
                {
                    option.choices.push_back({ "Off", "false", {} });
                    option.choices.push_back({ "On", "true", {} });
                }
            }
            info.options.emplace_back(std::move(option));
        }
        if (info.options.size() >= MAX_OPTIONS_PER_MOD)
            break;
    }
}
static bool IsSafeManagedFolderName(std::string_view folderName)
{
    if (folderName.empty() || folderName == "." || folderName == ".." ||
        folderName.find_first_of("/\\:") != std::string_view::npos)
        return false;

    const auto folderPath = PathFromUtf8(folderName);
    return IsSafeRelativeModPath(folderPath) && !folderPath.has_parent_path();
}

static void ParseReadOnlyList(std::string readOnly, Mod& mod)
{
    std::replace(readOnly.begin(), readOnly.end(), '\\', '/');
    size_t start = 0;

    while (start <= readOnly.size())
    {
        const size_t separator = readOnly.find(',', start);
        std::string item = readOnly.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front())))
            item.erase(item.begin());
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back())))
            item.pop_back();

        if (!item.empty())
        {
            const auto relativePath = PathFromUtf8(item);
            if (IsSafeRelativeModPath(relativePath))
                mod.readOnly.emplace(relativePath.lexically_normal());
        }

        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
}

static bool HasRootAssetDirectory(const std::filesystem::path& modDirectory, std::string_view directoryName)
{
    const auto assetDirectory = modDirectory / directoryName;
    if (!IsSafeManagedModPath(modDirectory, assetDirectory))
        return false;

    std::error_code error;
    if (std::filesystem::is_directory(assetDirectory, error))
        return true;

#if defined(__PROSPERO__)
    if (error == std::errc::operation_not_permitted || error == std::errc::permission_denied)
    {
        // Use directory enumeration as a fallback for PS5 filesystems that
        // deny metadata queries on otherwise accessible /data folders.
        error.clear();
        std::filesystem::directory_iterator probe(assetDirectory, error);
        return !error;
    }
#endif

    return false;
}

static bool HasRootGameAssetDirectories(const std::filesystem::path& modDirectory)
{
    return HasRootAssetDirectory(modDirectory, "xenon") || HasRootAssetDirectory(modDirectory, "win32");
}

static bool BuildManagedMod(
    const std::filesystem::path& modDirectory,
    const std::string& folderName,
    ManagedModCandidate& candidate)
{
    candidate = {};
    candidate.info.folderName = folderName;
    candidate.info.title = folderName;
    candidate.mod.type = ModType::UMM;
    candidate.mod.includeDirs.emplace_back(modDirectory);

    IniFile modIni;
    if (!modIni.read(modDirectory / "mod.ini"))
        return true; // A plain folder of loose files is a valid direct-override mod.

    candidate.info.title = modIni.getString("Desc", "Title", modIni.getString("Details", "Name", folderName));
    if (candidate.info.title.empty())
        candidate.info.title = folderName;
    candidate.info.description = modIni.getString("Desc", "Description", modIni.getString("Details", "Description", std::string()));
    candidate.info.author = modIni.getString("Desc", "Author", modIni.getString("Details", "Author", std::string()));
    ReadManagedModOptions(modDirectory, modIni, candidate.info);

    candidate.mod.includeDirs.clear();
    if (modIni.contains("Details") || modIni.contains("Filesystem")) // UMM-style mod.ini
    {
        candidate.mod.type = ModType::UMM;
        candidate.mod.includeDirs.emplace_back(modDirectory);
        candidate.mod.merge = modIni.getBool("Details", "Merge", modIni.getBool("Filesystem", "Merge", false));
        candidate.mod.customFilesystem = modIni.getBool(
            "Details", "CustomFilesystem", modIni.getBool("Filesystem", "CustomFilesystem", false));
        if (candidate.mod.merge)
            LOGF_WARNING("Managed mod '{}' requests archive merging; Marathon's archive-merge hooks are inactive, so merged archive assets will not be applied.", folderName);

        const std::string readOnly = modIni.getString(
            "Details", "Read-only", modIni.getString("Filesystem", "Read-only", std::string()));
        ParseReadOnlyList(readOnly, candidate.mod);

        const std::string saveFilePath = modIni.getString(
            "Details", "Save", modIni.getString("Filesystem", "Save", std::string()));
        if (!saveFilePath.empty())
        {
            std::string normalizedSavePath = saveFilePath;
            std::replace(normalizedSavePath.begin(), normalizedSavePath.end(), '\\', '/');
            const auto relativeSavePath = PathFromUtf8(normalizedSavePath);
            if (IsSafeRelativeModPath(relativeSavePath))
                candidate.saveFilePath = modDirectory / relativeSavePath;
        }
    }
    else // HMM-style mod.ini
    {
        candidate.mod.type = ModType::HMM;
        const size_t includeDirCount = modIni.get<size_t>("Main", "IncludeDirCount", 0);
        if (includeDirCount > MAX_MANAGED_MOD_COUNT)
        {
            LOGF_WARNING("Skipping managed mod '{}': IncludeDirCount is too large ({}).", folderName, includeDirCount);
            return false;
        }

        for (size_t i = 0; i < includeDirCount; ++i)
        {
            std::string includeDir = modIni.getString("Main", fmt::format("IncludeDir{}", i), std::string());
            if (includeDir.empty())
                continue;

            std::replace(includeDir.begin(), includeDir.end(), '\\', '/');
            const auto relativeIncludeDir = PathFromUtf8(includeDir);
            if (!IsSafeRelativeModPath(relativeIncludeDir))
            {
                LOGF_WARNING("Skipping managed mod '{}': IncludeDir{} is not a safe relative path.", folderName, i);
                return false;
            }

            const auto includePath = (modDirectory / relativeIncludeDir).lexically_normal();
            std::error_code pathError;
            if (!std::filesystem::is_directory(includePath, pathError) || pathError)
            {
                LOGF_WARNING("Managed mod '{}' has an unavailable include directory '{}'.", folderName, includePath.string());
                continue;
            }

            candidate.mod.includeDirs.emplace_back(includePath);
        }

        if (includeDirCount == 0)
            candidate.mod.includeDirs.emplace_back(modDirectory);
        else if (HasRootGameAssetDirectories(modDirectory))
        {
            // Keep root platform trees after configured HMM folders so
            // whole-tree overrides retain their established priority.
            const auto normalizedRoot = modDirectory.lexically_normal();
            const bool rootAlreadyIncluded = std::any_of(
                candidate.mod.includeDirs.begin(), candidate.mod.includeDirs.end(),
                [&](const std::filesystem::path& includeDir)
                {
                    return includeDir.lexically_normal() == normalizedRoot;
                });
            if (!rootAlreadyIncluded)
                candidate.mod.includeDirs.emplace_back(modDirectory);
        }

        std::string saveFilePath = modIni.getString("Main", "SaveFile", std::string());
        if (!saveFilePath.empty())
        {
            std::replace(saveFilePath.begin(), saveFilePath.end(), '\\', '/');
            const auto relativeSavePath = PathFromUtf8(saveFilePath);
            if (IsSafeRelativeModPath(relativeSavePath))
                candidate.saveFilePath = modDirectory / relativeSavePath / "SYS-DATA";
        }
    }

    return !candidate.mod.includeDirs.empty();
}

static bool SetIniTextProperty(std::string& contents, std::string_view sectionName, std::string_view propertyName, std::string_view value)
{
    if (!IsSafeIniId(sectionName) || !IsSafeIniId(propertyName) || !IsSafeIniValue(value))
        return false;

    const bool hasBom = contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF;
    if (hasBom)
        contents.erase(0, 3);

    std::vector<std::string> lines;
    std::istringstream input(contents);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == static_cast<char>(13))
            line.pop_back();
        lines.emplace_back(std::move(line));
    }

    bool inSection = false;
    bool sectionExists = false;
    bool propertyExists = false;
    size_t insertionIndex = lines.size();
    std::string propertyLine(propertyName);
    propertyLine += " = ";
    propertyLine.push_back(static_cast<char>(34));
    propertyLine.append(value);
    propertyLine.push_back(static_cast<char>(34));

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string trimmed = TrimAscii(lines[i]);
        if (!trimmed.empty() && trimmed.front() == '[')
        {
            if (inSection)
                insertionIndex = i;
            const size_t closeBracket = trimmed.find(']');
            const std::string_view currentSection = closeBracket == std::string::npos
                ? std::string_view{}
                : std::string_view(trimmed).substr(1, closeBracket - 1);
            inSection = currentSection == sectionName;
            if (inSection)
            {
                sectionExists = true;
                insertionIndex = i + 1;
            }
            continue;
        }

        if (!inSection || trimmed.empty() || trimmed.front() == ';')
            continue;
        const size_t equals = lines[i].find('=');
        if (equals == std::string::npos)
            continue;

        std::string existingName = TrimAscii(std::string_view(lines[i]).substr(0, equals));
        if (existingName.size() >= 2 && existingName.front() == static_cast<char>(34) && existingName.back() == static_cast<char>(34))
            existingName = existingName.substr(1, existingName.size() - 2);
        if (existingName == propertyName)
        {
            lines[i] = propertyLine;
            propertyExists = true;
        }
    }

    if (!propertyExists)
    {
        if (!sectionExists)
        {
            if (!lines.empty() && !lines.back().empty())
                lines.emplace_back();
            std::string sectionLine("[");
            sectionLine.append(sectionName);
            sectionLine.push_back(']');
            lines.emplace_back(std::move(sectionLine));
            insertionIndex = lines.size();
        }
        lines.insert(lines.begin() + std::min(insertionIndex, lines.size()), propertyLine);
    }

    std::ostringstream output;
    if (hasBom)
    {
        output.put(static_cast<char>(0xEF));
        output.put(static_cast<char>(0xBB));
        output.put(static_cast<char>(0xBF));
    }
    for (const auto& outputLine : lines)
        output << outputLine << static_cast<char>(10);
    contents = output.str();
    return true;
}

static bool WriteManagedTextFileAtomically(const std::filesystem::path& filePath, std::string_view contents, std::string& errorMessage)
{
    std::error_code error;
    std::filesystem::create_directories(filePath.parent_path(), error);
    if (error)
    {
        errorMessage = fmt::format("Could not create '{}': {}.", filePath.parent_path().string(), error.message());
        return false;
    }

    auto temporaryPath = filePath;
    temporaryPath += ".tmp";
    std::filesystem::remove(temporaryPath, error);
    error.clear();
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            errorMessage = fmt::format("Could not open '{}' for writing.", temporaryPath.string());
            return false;
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output)
        {
            errorMessage = fmt::format("Could not write '{}'.", temporaryPath.string());
            output.close();
            std::filesystem::remove(temporaryPath, error);
            return false;
        }
    }

    std::filesystem::rename(temporaryPath, filePath, error);
    if (error)
    {
        auto backupPath = filePath;
        backupPath += ".bak";
        std::error_code cleanupError;
        std::filesystem::remove(backupPath, cleanupError);
        std::error_code backupError;
        std::filesystem::rename(filePath, backupPath, backupError);
        if (backupError == std::errc::no_such_file_or_directory)
        {
            error.clear();
            std::filesystem::rename(temporaryPath, filePath, error);
        }
        else if (!backupError)
        {
            error.clear();
            std::filesystem::rename(temporaryPath, filePath, error);
            if (error)
            {
                std::error_code restoreError;
                std::filesystem::rename(backupPath, filePath, restoreError);
            }
            else
                std::filesystem::remove(backupPath, cleanupError);
        }
        else
            error = backupError;
    }

    if (error)
    {
        errorMessage = fmt::format("Could not replace '{}': {}.", filePath.string(), error.message());
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        return false;
    }
    return true;
}

struct ManagedConfigFileUpdate
{
    std::filesystem::path path;
    std::vector<std::tuple<std::string, std::string, std::string>> properties;
    bool existed = false;
    std::string originalContents;
    std::string newContents;
};

static void RollBackManagedConfigFiles(const std::vector<ManagedConfigFileUpdate>& updates, size_t writtenCount)
{
    while (writtenCount > 0)
    {
        const auto& update = updates[--writtenCount];
        if (update.existed)
        {
            std::string ignoredError;
            (void)WriteManagedTextFileAtomically(update.path, update.originalContents, ignoredError);
        }
        else
        {
            std::error_code ignoredError;
            std::filesystem::remove(update.path, ignoredError);
        }
    }
}

static bool WriteManagedModsManifest(const std::vector<UserModInfo>& mods, std::string& errorMessage)
{
    ModManifestJson manifest = g_managedManifestTemplate.is_object()
        ? g_managedManifestTemplate
        : ModManifestJson::object();
    manifest["version"] = 1;
    manifest["mods"] = ModManifestJson::array();

    for (const auto& mod : mods)
    {
        if (!IsSafeManagedFolderName(mod.folderName))
        {
            errorMessage = fmt::format("Refusing to save unsafe mod folder name '{}'.", mod.folderName);
            return false;
        }

        ModManifestJson entry = ModManifestJson::object();
        const auto existingEntry = g_managedModManifestTemplates.find(mod.folderName);
        if (existingEntry != g_managedModManifestTemplates.end() && existingEntry->second.is_object())
            entry = existingEntry->second;
        entry["folder"] = mod.folderName;
        entry["enabled"] = mod.enabled;
        manifest["mods"].push_back(std::move(entry));
    }

    std::error_code error;
    std::filesystem::create_directories(g_managedModsDirectory, error);
    if (error)
    {
        errorMessage = fmt::format("Could not create '{}': {}.", g_managedModsDirectory.string(), error.message());
        return false;
    }

    const auto manifestPath = g_managedModsDirectory / "mods.json";
    auto temporaryPath = manifestPath;
    temporaryPath += ".tmp";
    std::filesystem::remove(temporaryPath, error);
    error.clear();

    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            errorMessage = fmt::format("Could not open '{}' for writing.", temporaryPath.string());
            return false;
        }

        output << manifest.dump(2) << '\n';
        output.flush();
        if (!output)
        {
            errorMessage = fmt::format("Could not write '{}'.", temporaryPath.string());
            return false;
        }
    }

    std::filesystem::rename(temporaryPath, manifestPath, error);
    if (error)
    {
        // Windows does not replace an existing destination with rename(). Keep
        // a backup while retrying so a failed write never destroys the old file.
        auto backupPath = manifestPath;
        backupPath += ".bak";
        std::error_code cleanupError;
        std::filesystem::remove(backupPath, cleanupError);

        std::error_code backupError;
        std::filesystem::rename(manifestPath, backupPath, backupError);
        if (!backupError)
        {
            error.clear();
            std::filesystem::rename(temporaryPath, manifestPath, error);
            if (error)
            {
                std::error_code restoreError;
                std::filesystem::rename(backupPath, manifestPath, restoreError);
            }
            else
            {
                std::filesystem::remove(backupPath, cleanupError);
            }
        }
    }

    if (error)
    {
        errorMessage = fmt::format("Could not replace '{}': {}.", manifestPath.string(), error.message());
        std::filesystem::remove(temporaryPath, error);
        return false;
    }

    return true;
}

static void LoadManagedMods()
{
    g_userMods.clear();
    g_initialUserMods.clear();
    g_managedModsDirectory = ManagedModsDirectory();
    g_hasManagedSaveFilePath = false;
    g_managedManifestTemplate = ModManifestJson::object();
    g_managedModManifestTemplates.clear();

    std::error_code error;
    std::filesystem::create_directories(g_managedModsDirectory, error);
    if (error)
    {
        LOGF_WARNING("Could not create managed mods directory '{}': {}.", g_managedModsDirectory.string(), error.message());
        return;
    }

    LOGF_UTILITY("Scanning managed mods in '{}'.", g_managedModsDirectory.string());

    std::vector<ManagedModCandidate> discovered;
    std::filesystem::directory_iterator it(
        g_managedModsDirectory,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::directory_iterator end;

    for (; !error && it != end; it.increment(error))
    {
        std::error_code entryError;
#if defined(__PROSPERO__)
        // Some PS5 filesystem metadata operations can report EPERM for valid
        // /data entries. Follow the entry as a directory first, and if that
        // probe is denied, verify the expected root-level mod.ini directly.
        bool isDirectory = it->is_directory(entryError);
        if (entryError)
        {
            const std::error_code metadataError = entryError;
            entryError.clear();
            std::ifstream modIniProbe(it->path() / "mod.ini", std::ios::binary);
            isDirectory = static_cast<bool>(modIniProbe);
            if (!isDirectory)
                LOGF_WARNING("Could not inspect managed mod candidate '{}': {}.", it->path().string(), metadataError.message());
        }
        if (entryError || !isDirectory)
            continue;

        std::error_code symlinkError;
        const auto status = it->symlink_status(symlinkError);
        if ((!symlinkError && std::filesystem::is_symlink(status)) ||
            (symlinkError && symlinkError != std::errc::operation_not_permitted &&
                symlinkError != std::errc::permission_denied))
            continue;
#else
        const auto status = it->symlink_status(entryError);
        if (entryError || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status))
            continue;
#endif

        const std::string folderName = PathToUtf8(it->path().filename());
        if (!IsSafeManagedFolderName(folderName))
        {
            LOGF_WARNING("Skipping managed mod folder with unsafe name '{}'.", folderName);
            continue;
        }

        ManagedModCandidate candidate;
        if (!BuildManagedMod(it->path(), folderName, candidate))
        {
            LOGF_WARNING("Skipping managed mod folder '{}': no usable asset include directories were found.", folderName);
            continue;
        }

        LOGF_UTILITY("Detected managed mod folder '{}' at '{}'.", folderName, it->path().string());
        discovered.emplace_back(std::move(candidate));
        if (discovered.size() >= MAX_MANAGED_MOD_COUNT)
        {
            LOGF_WARNING("Managed mod scan reached the limit of {} folders.", MAX_MANAGED_MOD_COUNT);
            break;
        }
    }

    if (error)
        LOGF_WARNING("Managed mod scan stopped: {}.", error.message());

    std::sort(discovered.begin(), discovered.end(), [](const ManagedModCandidate& left, const ManagedModCandidate& right)
    {
        return left.info.folderName < right.info.folderName;
    });

    const auto manifestPath = g_managedModsDirectory / "mods.json";
    std::ifstream manifestInput(manifestPath, std::ios::binary);
    bool manifestWasReadable = static_cast<bool>(manifestInput);
    bool manifestWasValid = false;
    bool manifestNeedsWrite = !manifestWasReadable;
    std::vector<std::pair<std::string, bool>> savedOrder;

    if (manifestWasReadable)
    {
        ModManifestJson manifest = ModManifestJson::parse(manifestInput, nullptr, false);
        if (!manifest.is_discarded() && manifest.is_object() && manifest.contains("mods") && manifest["mods"].is_array())
        {
            manifestWasValid = true;
            g_managedManifestTemplate = manifest;
            g_managedManifestTemplate.erase("mods");
            std::unordered_set<std::string> seenFolders;
            for (const auto& entry : manifest["mods"])
            {
                std::string folderName;
                bool enabled = true;
                if (entry.is_string())
                {
                    folderName = entry.get<std::string>();
                }
                else if (entry.is_object())
                {
                    if (entry.contains("folder") && entry["folder"].is_string())
                        folderName = entry["folder"].get<std::string>();
                    else if (entry.contains("path") && entry["path"].is_string())
                        folderName = entry["path"].get<std::string>();
                    else if (entry.contains("id") && entry["id"].is_string())
                        folderName = entry["id"].get<std::string>();

                    if (entry.contains("enabled") && entry["enabled"].is_boolean())
                        enabled = entry["enabled"].get<bool>();
                }

                if (!IsSafeManagedFolderName(folderName) || !seenFolders.emplace(folderName).second)
                {
                    manifestNeedsWrite = true;
                    continue;
                }

                const auto candidate = std::find_if(discovered.begin(), discovered.end(), [&](const ManagedModCandidate& value)
                {
                    return value.info.folderName == folderName;
                });
                if (candidate == discovered.end())
                {
                    manifestNeedsWrite = true;
                    continue; // Drop folders that are no longer installed.
                }

                if (entry.is_object())
                    g_managedModManifestTemplates.emplace(folderName, entry);
                savedOrder.emplace_back(folderName, enabled);
            }

            if (savedOrder.size() != discovered.size())
                manifestNeedsWrite = true;
        }
        else
        {
            LOGF_WARNING("Ignoring invalid managed mods manifest '{}'; discovered mods will start enabled.", manifestPath.string());
        }

        manifestInput.close();
    }

    std::unordered_set<std::string> orderedFolders;
    std::vector<ManagedModCandidate> orderedMods;
    orderedMods.reserve(discovered.size());

    if (manifestWasValid)
    {
        for (const auto& [folderName, enabled] : savedOrder)
        {
            const auto candidate = std::find_if(discovered.begin(), discovered.end(), [&](const ManagedModCandidate& value)
            {
                return value.info.folderName == folderName;
            });
            if (candidate == discovered.end())
                continue;

            ManagedModCandidate ordered = *candidate;
            ordered.info.enabled = enabled;
            orderedFolders.emplace(folderName);
            orderedMods.emplace_back(std::move(ordered));
        }
    }

    for (const auto& candidate : discovered)
    {
        if (orderedFolders.emplace(candidate.info.folderName).second)
        {
            ManagedModCandidate ordered = candidate;
            ordered.info.enabled = true;
            orderedMods.emplace_back(std::move(ordered));
            if (manifestWasValid)
                manifestNeedsWrite = true;
        }
    }

    for (auto& candidate : orderedMods)
    {
        if (candidate.info.enabled && !g_hasManagedSaveFilePath && !candidate.saveFilePath.empty())
        {
            ModLoader::s_saveFilePath = candidate.saveFilePath;
            g_hasManagedSaveFilePath = true;
        }

        if (candidate.info.enabled)
            g_mods.emplace_back(std::move(candidate.mod));
        g_userMods.emplace_back(std::move(candidate.info));
    }

    if (!manifestWasValid && manifestWasReadable)
    {
        // Do not overwrite a readable but malformed JSON file automatically.
    }
    else if (manifestNeedsWrite)
    {
        std::string writeError;
        if (!WriteManagedModsManifest(g_userMods, writeError))
            LOGF_WARNING("Could not update managed mods manifest: {}.", writeError);
    }

    g_initialUserMods = g_userMods;
    LOGF_UTILITY("Loaded {} managed mod folder(s), {} enabled, from '{}'.",
        g_userMods.size(),
        std::count_if(g_userMods.begin(), g_userMods.end(), [](const UserModInfo& mod) { return mod.enabled; }),
        g_managedModsDirectory.string());
}

std::filesystem::path ModLoader::GetManagedModsDirectory()
{
    return ManagedModsDirectory();
}

const std::vector<UserModInfo>& ModLoader::GetUserMods()
{
    return g_userMods;
}

bool ModLoader::SetUserModEnabled(size_t modIndex, bool enabled, std::string& errorMessage)
{
    if (modIndex >= g_userMods.size())
    {
        errorMessage = "The selected mod is no longer in the managed list.";
        return false;
    }

    if (g_userMods[modIndex].enabled == enabled)
        return true;

    const bool oldValue = g_userMods[modIndex].enabled;
    g_userMods[modIndex].enabled = enabled;
    if (!WriteManagedModsManifest(g_userMods, errorMessage))
    {
        g_userMods[modIndex].enabled = oldValue;
        return false;
    }

    return true;
}

bool ModLoader::MoveUserMod(size_t modIndex, int direction, std::string& errorMessage)
{
    if (modIndex >= g_userMods.size() || (direction != -1 && direction != 1))
    {
        errorMessage = "The selected mod cannot be moved.";
        return false;
    }

    const auto nextIndex = static_cast<std::ptrdiff_t>(modIndex) + direction;
    if (nextIndex < 0 || nextIndex >= static_cast<std::ptrdiff_t>(g_userMods.size()))
        return false;

    std::swap(g_userMods[modIndex], g_userMods[static_cast<size_t>(nextIndex)]);
    if (!WriteManagedModsManifest(g_userMods, errorMessage))
    {
        std::swap(g_userMods[modIndex], g_userMods[static_cast<size_t>(nextIndex)]);
        return false;
    }

    return true;
}

bool ModLoader::CommitUserMods(const std::vector<UserModInfo>& mods, std::string& errorMessage)
{
    if (g_managedModsDirectory.empty())
    {
        errorMessage = "The managed mods directory is not available.";
        return false;
    }
    if (mods.size() != g_userMods.size())
    {
        errorMessage = "The mod list changed while the Mods window was open.";
        return false;
    }

    std::unordered_set<std::string> folderNames;
    std::vector<ManagedConfigFileUpdate> updates;
    for (const auto& mod : mods)
    {
        if (!IsSafeManagedFolderName(mod.folderName) || !folderNames.emplace(mod.folderName).second)
        {
            errorMessage = "The managed mod list contains an invalid or duplicate folder.";
            return false;
        }

        const auto originalMod = std::find_if(g_userMods.begin(), g_userMods.end(), [&](const UserModInfo& value)
        {
            return value.folderName == mod.folderName;
        });
        if (originalMod == g_userMods.end())
        {
            errorMessage = fmt::format("The mod folder '{}' is no longer available.", mod.folderName);
            return false;
        }
        if (mod.options.size() != originalMod->options.size())
        {
            errorMessage = fmt::format("The configuration schema for '{}' changed while the Mods window was open.", mod.folderName);
            return false;
        }

        const auto modDirectory = (g_managedModsDirectory / PathFromUtf8(mod.folderName)).lexically_normal();
        if (!IsSafeManagedModPath(g_managedModsDirectory, modDirectory))
        {
            errorMessage = fmt::format("The mod folder '{}' no longer resolves safely.", mod.folderName);
            return false;
        }

        for (const auto& option : mod.options)
        {
            const auto originalOption = std::find_if(originalMod->options.begin(), originalMod->options.end(), [&](const ModConfigOption& value)
            {
                return value.sectionName == option.sectionName && value.name == option.name && value.iniFile == option.iniFile;
            });
            if (originalOption == originalMod->options.end())
            {
                errorMessage = fmt::format("The option '{}' in '{}' is no longer available.", option.displayName, mod.folderName);
                return false;
            }
            if (option.value == originalOption->value)
                continue;

            const bool valueIsInSchema = std::any_of(originalOption->choices.begin(), originalOption->choices.end(), [&](const ModConfigChoice& choice)
            {
                return choice.value == option.value;
            });
            if (!valueIsInSchema || !IsSafeIniValue(option.value))
            {
                errorMessage = fmt::format("The selected value for '{}' is not a safe value in the mod schema.", option.displayName);
                return false;
            }

            const auto relativeIniPath = PathFromUtf8(option.iniFile);
            if (!IsSafeRelativeModPath(relativeIniPath))
            {
                errorMessage = fmt::format("The config file for '{}' is not a safe relative path.", mod.folderName);
                return false;
            }
            const auto targetPath = (modDirectory / relativeIniPath).lexically_normal();
            if (!IsSafeManagedModPath(modDirectory, targetPath) ||
                !IsSafeManagedModPath(g_managedModsDirectory, targetPath))
            {
                errorMessage = fmt::format("The config file for '{}' escapes its managed mod folder.", mod.folderName);
                return false;
            }

            auto update = std::find_if(updates.begin(), updates.end(), [&](const ManagedConfigFileUpdate& value)
            {
                return value.path == targetPath;
            });
            if (update == updates.end())
            {
                updates.emplace_back();
                updates.back().path = targetPath;
                update = std::prev(updates.end());
            }
            update->properties.emplace_back(option.sectionName, option.name, option.value);
        }
    }

    if (folderNames.size() != g_userMods.size())
    {
        errorMessage = "The managed mod list is incomplete.";
        return false;
    }

    for (auto& update : updates)
    {
        std::ifstream input(update.path, std::ios::binary);
        if (input)
        {
            update.existed = true;
            if (!ReadStreamLimited(input, update.originalContents, 4 * 1024 * 1024))
            {
                errorMessage = fmt::format("Could not read mod config file '{}' safely.", update.path.string());
                return false;
            }
        }

        update.newContents = update.originalContents;
        for (const auto& [sectionName, propertyName, value] : update.properties)
        {
            if (!SetIniTextProperty(update.newContents, sectionName, propertyName, value))
            {
                errorMessage = fmt::format("Could not update '{}' in '{}'.", propertyName, update.path.string());
                return false;
            }
        }
    }

    size_t writtenCount = 0;
    for (const auto& update : updates)
    {
        if (!IsSafeManagedModPath(g_managedModsDirectory, update.path))
        {
            RollBackManagedConfigFiles(updates, writtenCount);
            errorMessage = fmt::format("Refusing to write '{}' because it is outside the managed mods tree.", update.path.string());
            return false;
        }
        if (!WriteManagedTextFileAtomically(update.path, update.newContents, errorMessage))
        {
            RollBackManagedConfigFiles(updates, writtenCount);
            return false;
        }
        ++writtenCount;
    }

    if (!WriteManagedModsManifest(mods, errorMessage))
    {
        RollBackManagedConfigFiles(updates, writtenCount);
        return false;
    }

    g_userMods = mods;
    return true;
}

bool ModLoader::IsRestartRequired()
{
    if (g_userMods.size() != g_initialUserMods.size())
        return true;

    for (size_t i = 0; i < g_userMods.size(); ++i)
    {
        const auto& currentMod = g_userMods[i];
        const auto& initialMod = g_initialUserMods[i];
        if (currentMod.folderName != initialMod.folderName || currentMod.enabled != initialMod.enabled ||
            currentMod.options.size() != initialMod.options.size())
            return true;

        for (const auto& currentOption : currentMod.options)
        {
            const auto initialOption = std::find_if(initialMod.options.begin(), initialMod.options.end(), [&](const ModConfigOption& value)
            {
                return value.sectionName == currentOption.sectionName && value.name == currentOption.name && value.iniFile == currentOption.iniFile;
            });
            if (initialOption == initialMod.options.end() || initialOption->value != currentOption.value)
                return true;
        }
    }

    return false;
}

static bool IsManagedOverrideAvailable(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::exists(path, error))
        return true;

#if defined(__PROSPERO__)
    if (error == std::errc::operation_not_permitted || error == std::errc::permission_denied)
    {
        // PS5 may deny a metadata lookup for a valid /data override. Opening
        // the requested asset is the definitive availability check.
        std::ifstream overrideProbe(path, std::ios::binary);
        return overrideProbe.good();
    }
#endif

    return false;
}

static std::filesystem::path GetSingularArchiveAlias(const std::filesystem::path& path)
{
    const std::string pathUtf8 = PathToUtf8(path);
    constexpr std::array<std::pair<std::string_view, std::string_view>, 2> ARCHIVE_PREFIXES = {{
        { "xenon/archives/", "xenon/archive/" },
        { "win32/archives/", "win32/archive/" }
    }};

    for (const auto& [pluralPrefix, singularPrefix] : ARCHIVE_PREFIXES)
    {
        if (!std::string_view(pathUtf8).starts_with(pluralPrefix))
            continue;

        std::string aliasUtf8(singularPrefix);
        aliasUtf8.append(pathUtf8.substr(pluralPrefix.size()));
        return PathFromUtf8(aliasUtf8);
    }

    return {};
}

static std::filesystem::path GetCustomFilesystemMarkerAlias(const std::filesystem::path& path)
{
    const std::string pathUtf8 = PathToUtf8(path);
    const size_t separator = pathUtf8.find_last_of('/');
    const size_t nameOffset = separator == std::string::npos ? 0 : separator + 1;
    if (nameOffset >= pathUtf8.size() || pathUtf8[nameOffset] == '#')
        return {};

    std::string aliasUtf8 = pathUtf8;
    aliasUtf8.insert(nameOffset, "#");
    return PathFromUtf8(aliasUtf8);
}

static std::string LowerAsciiString(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

static bool IsAsciiPackageString(std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char character)
    {
        return character >= 0x20 && character <= 0x7E;
    });
}

static std::vector<CustomArchiveCandidate> CollectCustomArchiveCandidates()
{
    std::vector<CustomArchiveCandidate> candidates;
    std::unordered_set<std::string> seenPackagePaths;

    for (const auto& mod : g_mods)
    {
        if (!mod.customFilesystem)
            continue;

        for (const auto& includeDir : mod.includeDirs)
        {
            std::error_code error;
            std::filesystem::recursive_directory_iterator iterator(
                includeDir,
                std::filesystem::directory_options::skip_permission_denied,
                error);
            const std::filesystem::recursive_directory_iterator end;
            if (error)
                continue;

            while (iterator != end)
            {
                const auto entry = *iterator;
                std::error_code entryError;
                const auto status = entry.symlink_status(entryError);
                if (!entryError && std::filesystem::is_symlink(status))
                    iterator.disable_recursion_pending();

                iterator.increment(error);
                if (error)
                {
                    error.clear();
                    continue;
                }
                if (entryError || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status))
                    continue;

                const std::string extension = LowerAsciiString(PathToUtf8(entry.path().extension()));
                if (extension != ".arc")
                    continue;

                const std::filesystem::path relativePath = entry.path().lexically_relative(includeDir).lexically_normal();
                std::vector<std::string> components;
                for (const auto& component : relativePath)
                {
                    if (component == "." || component.empty())
                        continue;
                    components.emplace_back(PathToUtf8(component));
                }
                if (components.size() < 3)
                    continue;

                const std::string platformLower = LowerAsciiString(components[0]);
                if (platformLower != "xenon" && platformLower != "win32")
                    continue;

                const std::string archiveDirectoryLower = LowerAsciiString(components[1]);
                if (archiveDirectoryLower != "archives" && archiveDirectoryLower != "archive")
                    continue;

                std::string archiveFileName = components.back();
                if (!archiveFileName.empty() && archiveFileName.front() == '#')
                    archiveFileName.erase(archiveFileName.begin());
                if (archiveFileName.empty() || archiveFileName.front() == '+')
                    continue; // +*.arc files are merge payloads, not standalone custom archives.

                const size_t extensionOffset = archiveFileName.find_last_of('.');
                if (extensionOffset == std::string::npos ||
                    LowerAsciiString(std::string_view(archiveFileName).substr(extensionOffset)) != ".arc")
                    continue;

                std::string archiveName = archiveFileName.substr(0, extensionOffset);
                if (archiveName.empty() || !IsAsciiPackageString(archiveName) || !IsAsciiPackageString(archiveFileName))
                {
                    LOGF_WARNING("Skipping a custom filesystem archive with a non-ASCII or empty name in '{}'.", includeDir.string());
                    continue;
                }

                CustomArchiveCandidate candidate;
                candidate.platform = components[0];
                candidate.category = platformLower == "win32" ? "archive_win32" : "archive";
                candidate.name = std::move(archiveName);
                candidate.packagePath = "archives/";
                for (size_t componentIndex = 2; componentIndex + 1 < components.size(); ++componentIndex)
                {
                    candidate.packagePath += components[componentIndex];
                    candidate.packagePath.push_back('/');
                }
                candidate.packagePath += archiveFileName;
                if (!IsAsciiPackageString(candidate.packagePath))
                {
                    LOGF_WARNING("Skipping custom filesystem archive '{}' because its package path is not ASCII.", entry.path().string());
                    continue;
                }

                const std::string dedupeKey = candidate.category + "/" + LowerAsciiString(candidate.packagePath);
                if (!seenPackagePaths.emplace(dedupeKey).second)
                    continue;

                // A leading # is the Sonic '06 Mod Manager's per-file custom marker;
                // CustomFilesystem=True also allows unmarked archives absent from archive.pkg.
                candidates.emplace_back(std::move(candidate));
            }
        }
    }

    return candidates;
}

static std::filesystem::path FindSystemArchiveSource(std::string_view platform)
{
    const std::string relativePathUtf8 = std::string(platform) + "/archives/system.arc";
    const std::filesystem::path relativePath = PathFromUtf8(relativePathUtf8);
    const std::filesystem::path singularAlias = GetSingularArchiveAlias(relativePath);

    // Preserve full plural-path priority before accepting the legacy singular folder.
    for (const auto& mod : g_mods)
    {
        for (const auto& includeDir : mod.includeDirs)
        {
            const std::filesystem::path modPath = includeDir / relativePath;
            if (IsManagedOverrideAvailable(modPath))
                return modPath;
        }
    }

    if (!singularAlias.empty())
    {
        for (const auto& mod : g_mods)
        {
            for (const auto& includeDir : mod.includeDirs)
            {
                const std::filesystem::path modPath = includeDir / singularAlias;
                if (IsManagedOverrideAvailable(modPath))
                    return modPath;
            }
        }
    }

    const std::filesystem::path gamePath = GetGamePath() / relativePath;
    if (IsManagedOverrideAvailable(gamePath))
        return gamePath;

    if (!singularAlias.empty())
    {
        const std::filesystem::path gameAliasPath = GetGamePath() / singularAlias;
        if (IsManagedOverrideAvailable(gameAliasPath))
            return gameAliasPath;
    }

    return {};
}

static void PrepareCustomFilesystemRegistry()
{
    std::lock_guard lock(g_customFilesystemMutex);
    if (g_customFilesystemPrepared)
        return;
    g_customFilesystemPrepared = true;

    if (std::none_of(g_mods.begin(), g_mods.end(), [](const Mod& mod)
        {
            return mod.customFilesystem;
        }))
        return;

#if defined(__PROSPERO__)
    // Do not redirect the live PS5 game to a rewritten system.arc until the
    // overlay has been validated against a retail archive. Direct file
    // overrides from both xenon/ and win32/ remain available through ResolvePath.
    LOG_WARNING("CustomFilesystem archive registration is temporarily disabled on PS5; direct xenon/ and win32/ mod files remain enabled.");
    return;
#endif

    const std::vector<CustomArchiveCandidate> candidates = CollectCustomArchiveCandidates();
    if (candidates.empty())
        return;

    const std::array<std::string_view, 2> platforms = { "xenon", "win32" };
    for (const std::string_view platform : platforms)
    {
        const std::filesystem::path systemArchivePath = FindSystemArchiveSource(platform);
        if (systemArchivePath.empty())
            continue;

        CustomFilesystem::ArchivePackage package;
        std::string errorMessage;
        if (!CustomFilesystem::ReadArchivePackage(systemArchivePath, platform, package, errorMessage))
        {
            LOGF_WARNING("Custom filesystem archives could not be registered for '{}': {}", platform, errorMessage);
            continue;
        }

        size_t addedCount = 0;
        std::unordered_set<std::string> missingCategories;
        for (const auto& candidate : candidates)
        {
            if (LowerAsciiString(candidate.platform) != platform)
                continue;

            const auto category = std::find_if(package.categories.begin(), package.categories.end(), [&](const CustomFilesystem::ArchiveCategory& value)
            {
                return LowerAsciiString(value.name) == candidate.category;
            });
            if (category == package.categories.end())
            {
                if (missingCategories.emplace(candidate.category).second)
                    LOGF_WARNING("The '{}' category is missing from {}'s archive.pkg; custom archives for that category were skipped.", candidate.category, platform);
                continue;
            }

            if (CustomFilesystem::HasArchivePackageEntry(package, candidate.category, candidate.name, candidate.packagePath))
                continue;

            category->files.push_back({ candidate.name, candidate.packagePath });
            ++addedCount;
        }

        if (addedCount == 0)
            continue;

        const std::filesystem::path outputPath = GetUserPath() / "modcache" / "custom-filesystem" /
            PathFromUtf8(std::string(platform) + "_system.arc");
        errorMessage.clear();
        if (!CustomFilesystem::WriteSystemArchiveOverlay(systemArchivePath, outputPath, platform, package, errorMessage))
        {
            LOGF_WARNING("Custom filesystem archive registration failed for '{}': {}", platform, errorMessage);
            continue;
        }

        std::string overrideKey = std::string(platform) + "/archives/system.arc";
        g_customSystemArchiveOverrides.emplace(LowerAsciiString(overrideKey), outputPath);
        LOGF_UTILITY("Mod Loader: Registered {} custom archive(s) in {}'s runtime archive.pkg overlay.", addedCount, platform);
    }
}

std::filesystem::path ModLoader::ResolvePath(std::string_view path)
{
    std::string_view root;

    size_t sepIndex = path.find(":\\");
    if (sepIndex != std::string_view::npos)
    {
        root = path.substr(0, sepIndex);
        path.remove_prefix(sepIndex + 2);
    }

    if (root == "save")
    {
        if (!ModLoader::s_saveFilePath.empty())
        {
            if (path == "SYS-DATA")
                return ModLoader::s_saveFilePath;
            else
                return ModLoader::s_saveFilePath.parent_path() / path;
        }

        return {};
    }

    if (g_mods.empty())
        return {};

    thread_local xxHashMap<std::filesystem::path> s_cache;

    XXH64_hash_t hash = XXH3_64bits(path.data(), path.size());
    auto findResult = s_cache.find(hash);
    if (findResult != s_cache.end())
        return findResult->second;

    std::string pathStr(path);
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
    std::filesystem::path fsPath(std::move(pathStr));

    const auto customSystemArchive = g_customSystemArchiveOverrides.find(LowerAsciiString(PathToUtf8(fsPath)));
    if (customSystemArchive != g_customSystemArchiveOverrides.end())
        return s_cache.emplace(hash, customSystemArchive->second).first->second;

    bool canBeMerged =
        path.find(".arl") == (path.size() - 4) ||
        path.find(".ar.") == (path.size() - 6) ||
        path.find(".ar") == (path.size() - 3);

    const std::filesystem::path singularArchiveAlias = GetSingularArchiveAlias(fsPath);
    const std::filesystem::path customMarkerAlias = GetCustomFilesystemMarkerAlias(fsPath);
    const std::filesystem::path singularCustomMarkerAlias = singularArchiveAlias.empty()
        ? std::filesystem::path{}
        : GetCustomFilesystemMarkerAlias(singularArchiveAlias);

    auto isModEligible = [&](const Mod& mod)
    {
        return !(mod.type == ModType::UMM && mod.merge && canBeMerged && !mod.readOnly.contains(fsPath));
    };

    // Keep exact plural paths ahead of every compatibility alias, even if the
    // exact file belongs to a later mod in the configured order.
    for (const auto& mod : g_mods)
    {
        if (!isModEligible(mod))
            continue;
        for (const auto& includeDir : mod.includeDirs)
        {
            const std::filesystem::path modPath = includeDir / fsPath;
            if (IsManagedOverrideAvailable(modPath))
                return s_cache.emplace(hash, modPath).first->second;
        }
    }

    // Sonic '06 Mod Manager marks files that should lose their leading # when installed.
    // Keep this alias separate from Merge so CustomFilesystem=False mods are unaffected.
    if (!customMarkerAlias.empty())
    {
        for (const auto& mod : g_mods)
        {
            if (!mod.customFilesystem || !isModEligible(mod))
                continue;
            for (const auto& includeDir : mod.includeDirs)
            {
                const std::filesystem::path modPath = includeDir / customMarkerAlias;
                if (IsManagedOverrideAvailable(modPath))
                    return s_cache.emplace(hash, modPath).first->second;
            }
        }
    }

    // A few distributed mods use xenon/archive/ (singular), while the retail
    // XEX requests xenon/archives/ (plural). Accept it only after exact paths.
    if (!singularArchiveAlias.empty())
    {
        for (const auto& mod : g_mods)
        {
            if (!isModEligible(mod))
                continue;
            for (const auto& includeDir : mod.includeDirs)
            {
                const std::filesystem::path modPath = includeDir / singularArchiveAlias;
                if (IsManagedOverrideAvailable(modPath))
                    return s_cache.emplace(hash, modPath).first->second;
            }
        }
    }

    if (!singularCustomMarkerAlias.empty())
    {
        for (const auto& mod : g_mods)
        {
            if (!mod.customFilesystem || !isModEligible(mod))
                continue;
            for (const auto& includeDir : mod.includeDirs)
            {
                const std::filesystem::path modPath = includeDir / singularCustomMarkerAlias;
                if (IsManagedOverrideAvailable(modPath))
                    return s_cache.emplace(hash, modPath).first->second;
            }
        }
    }

    return s_cache.emplace(hash, std::filesystem::path{}).first->second;
}

std::vector<std::filesystem::path>* ModLoader::GetIncludeDirectories(size_t modIndex)
{
    return modIndex < g_mods.size() ? &g_mods[modIndex].includeDirs : nullptr;
}

void ModLoader::Init()
{
    {
        std::lock_guard lock(g_customFilesystemMutex);
        g_customFilesystemPrepared = false;
        g_customSystemArchiveOverrides.clear();
    }

    g_mods.clear();
    s_saveFilePath.clear();
    s_isLogTypeConsole = false;
    LoadManagedMods();

    const std::filesystem::path& userPath = GetUserPath();

    IniFile configIni;
    if (!configIni.read(userPath / "cpkredir.ini"))
    {
        configIni = {};

        if (!configIni.read(GetGamePath() / "cpkredir.ini"))
        {
            PrepareCustomFilesystemRegistry();
            return;
        }
    }

    if (!configIni.getBool("CPKREDIR", "Enabled", true))
    {
        PrepareCustomFilesystemRegistry();
        return;
    }

    if (!g_hasManagedSaveFilePath && configIni.getBool("CPKREDIR", "EnableSaveFileRedirection", false))
    {
        std::string saveFilePathU8 = configIni.getString("CPKREDIR", "SaveFileFallback", "");
        if (!saveFilePathU8.empty())
            ModLoader::s_saveFilePath = std::u8string_view((const char8_t*)saveFilePathU8.c_str());
        else
            ModLoader::s_saveFilePath = userPath / "mlsave";

        ModLoader::s_saveFilePath /= "SYS-DATA";
    }

    if (configIni.getString("CPKREDIR", "LogType", std::string()) == "console")
    {
        os::process::ShowConsole();
        s_isLogTypeConsole = true;
    }

    std::string modsDbIniFilePathU8 = configIni.getString("CPKREDIR", "ModsDbIni", "");
    if (modsDbIniFilePathU8.empty())
    {
        PrepareCustomFilesystemRegistry();
        return;
    }

    IniFile modsDbIni;
    if (!modsDbIni.read(std::u8string_view((const char8_t*)modsDbIniFilePathU8.c_str())))
    {
        PrepareCustomFilesystemRegistry();
        return;
    }

    bool foundModSaveFilePath = g_hasManagedSaveFilePath;

    size_t activeModCount = modsDbIni.get<size_t>("Main", "ActiveModCount", 0);
    for (size_t i = 0; i < activeModCount; ++i)
    {
        std::string modId = modsDbIni.getString("Main", fmt::format("ActiveMod{}", i), "");
        if (modId.empty())
            continue;

        std::string modIniFilePathU8 = modsDbIni.getString("Mods", modId, "");
        if (modIniFilePathU8.empty())
            continue;

        std::filesystem::path modIniFilePath(std::u8string_view((const char8_t*)modIniFilePathU8.c_str()));

        IniFile modIni;
        if (!modIni.read(modIniFilePath))
            continue;

        auto modDirectoryPath = modIniFilePath.parent_path();
        std::string modSaveFilePathU8;

        Mod mod;

        if (modIni.contains("Details") || modIni.contains("Filesystem")) // UMM
        {
            mod.type = ModType::UMM;
            mod.includeDirs.emplace_back(modDirectoryPath);
            mod.merge = modIni.getBool("Details", "Merge", modIni.getBool("Filesystem", "Merge", false));
            mod.customFilesystem = modIni.getBool(
                "Details", "CustomFilesystem", modIni.getBool("Filesystem", "CustomFilesystem", false));

            std::string readOnly = modIni.getString("Details", "Read-only", modIni.getString("Filesystem", "Read-only", std::string()));
            std::replace(readOnly.begin(), readOnly.end(), '\\', '/');
            std::string_view readOnlySplit = readOnly;

            while (!readOnlySplit.empty())
            {
                size_t index = readOnlySplit.find(',');
                if (index == std::string_view::npos)
                {
                    mod.readOnly.emplace(readOnlySplit);
                    break;
                }

                mod.readOnly.emplace(readOnlySplit.substr(0, index));
                readOnlySplit.remove_prefix(index + 1);
            }

            if (!foundModSaveFilePath)
                modSaveFilePathU8 = modIni.getString("Details", "Save", modIni.getString("Filesystem", "Save", std::string()));
        }
        else // HMM
        {
            mod.type = ModType::HMM;

            size_t includeDirCount = modIni.get<size_t>("Main", "IncludeDirCount", 0);
            for (size_t j = 0; j < includeDirCount; j++)
            {
                std::string includeDirU8 = modIni.getString("Main", fmt::format("IncludeDir{}", j), "");
                if (!includeDirU8.empty())
                {
                    std::replace(includeDirU8.begin(), includeDirU8.end(), '\\', '/');
                    mod.includeDirs.emplace_back(modDirectoryPath / std::u8string_view((const char8_t*)includeDirU8.c_str()));
                }
            }

            if (includeDirCount == 0)
                mod.includeDirs.emplace_back(modDirectoryPath);
            else if (HasRootGameAssetDirectories(modDirectoryPath))
            {
                const auto normalizedRoot = modDirectoryPath.lexically_normal();
                const bool rootAlreadyIncluded = std::any_of(
                    mod.includeDirs.begin(), mod.includeDirs.end(),
                    [&](const std::filesystem::path& includeDir)
                    {
                        return includeDir.lexically_normal() == normalizedRoot;
                    });
                if (!rootAlreadyIncluded)
                    mod.includeDirs.emplace_back(modDirectoryPath);
            }

            if (!foundModSaveFilePath)
                modSaveFilePathU8 = modIni.getString("Main", "SaveFile", std::string());
        }

        if (!modSaveFilePathU8.empty())
        {
            std::replace(modSaveFilePathU8.begin(), modSaveFilePathU8.end(), '\\', '/');
            ModLoader::s_saveFilePath = modDirectoryPath / std::u8string_view((const char8_t*)modSaveFilePathU8.c_str());

            // Save file paths in HMM mods are treated as folders.
            if (mod.type == ModType::HMM)
                ModLoader::s_saveFilePath /= "SYS-DATA";

            foundModSaveFilePath = true;
        }

        if (!mod.includeDirs.empty())
            g_mods.emplace_back(std::move(mod));
    }

    auto codeCount = modsDbIni.get<size_t>("Codes", "CodeCount", 0);

    if (codeCount)
    {
        std::vector<std::string> codes{};

        for (size_t i = 0; i < codeCount; i++)
        {
            auto name = modsDbIni.getString("Codes", fmt::format("Code{}", i), "");

            if (name.empty())
                continue;

            codes.push_back(name);
        }

        for (auto& def : g_configDefinitions)
        {
            if (!def->IsHidden() || def->GetSection() != "Codes")
                continue;

            /* NOTE: this is inefficient, but it happens
               once on boot for a handful of codes at release
               and is temporary until we support real code mods. */
            for (size_t i = 0; i < codes.size(); i++)
            {
                if (def->GetName() == codes[i])
                {
                    LOGF_UTILITY("Mod Loader", "Loading code: \"{}\"", codes[i]);
                    *(bool*)def->GetValue() = true;
                    break;
                }
            }
        }
    }

    PrepareCustomFilesystemRegistry();
}

static constexpr uint32_t LZX_SIGNATURE = 0xFF512EE;

static std::span<uint8_t> decompressLzx(PPCContext& ctx, uint8_t* base, const uint8_t* compressedData, size_t compressedDataSize, be<uint32_t>* scratchSpace)
{
    assert(g_memory.IsInMemoryRange(compressedData));

    bool shouldFreeScratchSpace = false;
    if (scratchSpace == nullptr)
    {
        scratchSpace = reinterpret_cast<be<uint32_t>*>(g_userHeap.Alloc(sizeof(uint32_t) * 2));
        shouldFreeScratchSpace = true;
    }

    // Initialize decompressor
    ctx.r3.u32 = 1;
    ctx.r4.u32 = uint32_t((compressedData + 0xC) - base);
    ctx.r5.u32 = *reinterpret_cast<const be<uint32_t>*>(compressedData + 0x8);
    ctx.r6.u32 = uint32_t(reinterpret_cast<uint8_t*>(scratchSpace) - base);
    // sub_831CE1A0(ctx, base);

    uint64_t decompressedDataSize = *reinterpret_cast<const be<uint64_t>*>(compressedData + 0x18);
    uint8_t* decompressedData = reinterpret_cast<uint8_t*>(g_userHeap.Alloc(decompressedDataSize));

    uint32_t blockSize = *reinterpret_cast<const be<uint32_t>*>(compressedData + 0x28);
    size_t decompressedDataOffset = 0;
    size_t compressedDataOffset = 0x30;

    while (decompressedDataOffset < decompressedDataSize)
    {
        size_t decompressedBlockSize = decompressedDataSize - decompressedDataOffset;

        if (decompressedBlockSize > blockSize)
            decompressedBlockSize = blockSize;

        *(scratchSpace + 1) = decompressedBlockSize;

        uint32_t compressedBlockSize = *reinterpret_cast<const be<uint32_t>*>(compressedData + compressedDataOffset);

        // Decompress
        ctx.r3.u32 = *scratchSpace;
        ctx.r4.u32 = uint32_t((decompressedData + decompressedDataOffset) - base);
        ctx.r5.u32 = uint32_t(reinterpret_cast<uint8_t*>(scratchSpace + 1) - base);
        ctx.r6.u32 = uint32_t((compressedData + compressedDataOffset + 0x4) - base);
        ctx.r7.u32 = compressedBlockSize;
        // sub_831CE0D0(ctx, base);

        decompressedDataOffset += *(scratchSpace + 1);
        compressedDataOffset += 0x4 + compressedBlockSize;
    }

    // Deinitialize decompressor
    ctx.r3.u32 = *scratchSpace;
    // sub_831CE150(ctx, base);

    if (shouldFreeScratchSpace)
        g_userHeap.Free(scratchSpace);

    return { decompressedData, decompressedDataSize };
}

// Hedgehog::Database::CDatabaseLoader::ReadArchiveList
// PPC_FUNC_IMPL(__imp__sub_82E0D3E8);
// PPC_FUNC(sub_82E0D3E8)
// {
//     if (g_mods.empty())
//     {
//         __imp__sub_82E0D3E8(ctx, base);
//         return;
//     }

//     thread_local ankerl::unordered_dense::set<std::string> s_fileNames;
//     s_fileNames.clear();

//     auto parseArlFileData = [&](const uint8_t* arlFileData, size_t arlFileSize)
//         {
//             struct ArlHeader
//             {
//                 uint32_t signature;
//                 uint32_t splitCount;
//             };

//             auto* arlHeader = reinterpret_cast<const ArlHeader*>(arlFileData);
//             size_t arlHeaderSize = sizeof(ArlHeader) + arlHeader->splitCount * sizeof(uint32_t);
//             const uint8_t* arlFileNames = arlFileData + arlHeaderSize;

//             while (arlFileNames < arlFileData + arlFileSize)
//             {
//                 uint8_t fileNameSize = *arlFileNames;
//                 ++arlFileNames;

//                 s_fileNames.emplace(reinterpret_cast<const char*>(arlFileNames), fileNameSize);

//                 arlFileNames += fileNameSize;
//             }

//             return arlHeaderSize;
//         };

//     auto parseArFileData = [&](const uint8_t* arFileData, size_t arFileSize)
//         {
//             struct ArEntry
//             {
//                 uint32_t entrySize;
//                 uint32_t dataSize;
//                 uint32_t dataOffset;
//                 uint32_t fileDateLow;
//                 uint32_t fileDateHigh;
//             };

//             for (size_t i = 16; i < arFileSize; )
//             {
//                 auto entry = reinterpret_cast<const ArEntry*>(arFileData + i);
//                 s_fileNames.emplace(reinterpret_cast<const char*>(entry + 1));
//                 i += entry->entrySize;
//             }
//         };

//     auto r3 = ctx.r3;
//     auto r4 = ctx.r4;
//     auto r5 = ctx.r5;
//     auto r6 = ctx.r6;

//     auto loadFile = [&]<typename TFunction>(const std::filesystem::path& filePath, const TFunction& function)
//     {
//         std::ifstream stream(filePath, std::ios::binary);
//         if (stream.good())
//         {
//             if (ModLoader::s_isLogTypeConsole)
//                 LOGF_IMPL(Utility, "Mod Loader", "Loading file: \"{}\"", reinterpret_cast<const char*>(filePath.u8string().c_str()));

//             be<uint32_t> signature{};
//             stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));

//             stream.seekg(0, std::ios::end);
//             size_t arlFileSize = stream.tellg();
//             stream.seekg(0, std::ios::beg);

//             if (signature == LZX_SIGNATURE)
//             {
//                 void* compressedFileData = g_userHeap.Alloc(arlFileSize);
//                 stream.read(reinterpret_cast<char*>(compressedFileData), arlFileSize);
//                 stream.close();

//                 auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(compressedFileData), arlFileSize, nullptr);

//                 g_userHeap.Free(compressedFileData);

//                 function(fileData.data(), fileData.size());

//                 g_userHeap.Free(fileData.data());
//             }
//             else
//             {
//                 thread_local std::vector<uint8_t> s_fileData;

//                 s_fileData.resize(arlFileSize);
//                 stream.read(reinterpret_cast<char*>(s_fileData.data()), arlFileSize);
//                 stream.close();

//                 function(s_fileData.data(), arlFileSize);
//             }

//             return true;
//         }

//         return false;
//     };

//     thread_local xxHashMap<std::vector<std::pair<std::filesystem::path, bool>>> s_cache;

//     std::u8string_view arlFilePathU8(reinterpret_cast<const char8_t*>(base + PPC_LOAD_U32(ctx.r4.u32)));
//     XXH64_hash_t hash = XXH3_64bits(arlFilePathU8.data(), arlFilePathU8.size());
//     auto findResult = s_cache.find(hash);

//     if (findResult != s_cache.end())
//     {
//         for (const auto& [arlFilePath, isArchiveList] : findResult->second)
//         {
//             if (isArchiveList)
//                 loadFile(arlFilePath, parseArlFileData);
//             else
//                 loadFile(arlFilePath, parseArFileData);
//         }
//     }
//     else
//     {
//         std::vector<std::pair<std::filesystem::path, bool>> arlFilePaths;
//         std::filesystem::path arlFilePath;
//         std::filesystem::path arFilePath;
//         std::filesystem::path appendArlFilePath;

//         for (auto& mod : g_mods)
//         {
//             for (auto& includeDir : mod.includeDirs)
//             {
//                 auto loadUncachedFile = [&](const std::filesystem::path& filePath, bool isArchiveList)
//                 {
//                     if (mod.type == ModType::UMM && mod.readOnly.contains(filePath))
//                         return false;

//                     std::filesystem::path combinedFilePath = includeDir / filePath;

//                     bool success;
//                     if (isArchiveList)
//                         success = loadFile(combinedFilePath, parseArlFileData);
//                     else
//                         success = loadFile(combinedFilePath, parseArFileData);

//                     if (success)
//                         arlFilePaths.emplace_back(std::move(combinedFilePath), isArchiveList);

//                     return success;
//                 };

//                 if (mod.type == ModType::UMM)
//                 {
//                     if (mod.merge)
//                     {
//                         if (arlFilePath.empty())
//                         {
//                             arlFilePath = arlFilePathU8;
//                             arlFilePath += ".arl";
//                         }

//                         if (!loadUncachedFile(arlFilePath, true))
//                         {
//                             if (arFilePath.empty())
//                             {
//                                 arFilePath = arlFilePathU8;
//                                 arFilePath += ".ar";
//                             }

//                             if (!loadUncachedFile(arFilePath, false))
//                             {
//                                 thread_local std::filesystem::path s_tempPath;

//                                 for (uint32_t i = 0; ; i++)
//                                 {
//                                     s_tempPath = arFilePath;
//                                     s_tempPath += fmt::format(".{:02}", i);

//                                     if (!loadUncachedFile(s_tempPath, false))
//                                         break;
//                                 }
//                             }
//                         }
//                     }
//                 }
//                 else if (mod.type == ModType::HMM)
//                 {
//                     if (appendArlFilePath.empty())
//                     {
//                         if (arlFilePath.empty())
//                         {
//                             arlFilePath = arlFilePathU8;
//                             arlFilePath += ".arl";
//                         }

//                         appendArlFilePath = arlFilePath.parent_path();
//                         appendArlFilePath /= "+";
//                         appendArlFilePath += arlFilePath.filename();
//                     }

//                     loadUncachedFile(appendArlFilePath, true);
//                 }
//             }
//         }

//         s_cache.emplace(hash, std::move(arlFilePaths));
//     }

//     ctx.r3 = r3;
//     ctx.r4 = r4;
//     ctx.r5 = r5;
//     ctx.r6 = r6;

//     if (s_fileNames.empty())
//     {
//         __imp__sub_82E0D3E8(ctx, base);
//         return;
//     }

//     size_t arlHeaderSize = parseArlFileData(base + ctx.r5.u32, ctx.r6.u32);
//     size_t arlFileSize = arlHeaderSize;

//     for (auto& fileName : s_fileNames)
//     {
//         arlFileSize += 1;
//         arlFileSize += fileName.size();
//     }

//     uint8_t* newArlFileData = reinterpret_cast<uint8_t*>(g_userHeap.Alloc(arlFileSize));
//     memcpy(newArlFileData, base + ctx.r5.u32, arlHeaderSize);

//     uint8_t* arlFileNames = newArlFileData + arlHeaderSize;
//     for (auto& fileName : s_fileNames)
//     {
//         *arlFileNames = uint8_t(fileName.size());
//         ++arlFileNames;
//         memcpy(arlFileNames, fileName.data(), fileName.size());
//         arlFileNames += fileName.size();
//     }

//     ctx.r5.u32 = uint32_t(newArlFileData - base);
//     ctx.r6.u32 = uint32_t(arlFileSize);

//     __imp__sub_82E0D3E8(ctx, base);

//     g_userHeap.Free(newArlFileData);
// }

// Load elements have an unused "pretty name" field. We will use this field to store the archive file path,
// prefixed with a magic string. When the first load detects this string, it will load append archives
// and then clear the field to prevent remaining splits from loading the append archives again.
// We cannot rely on .ar.00 being the first split to be loaded, so this approach is necessary.
static thread_local uint32_t g_prefixedArFilePath = 0;

// Hedgehog::Database::CDatabaseLoader::LoadArchives
// PPC_FUNC_IMPL(__imp__sub_82E0CC38);
// PPC_FUNC(sub_82E0CC38)
// {
//     if (g_mods.empty())
//     {
//         __imp__sub_82E0CC38(ctx, base);
//         return;
//     }

//     auto r3 = ctx.r3;
//     auto r4 = ctx.r4;
//     auto r5 = ctx.r5;
//     auto r6 = ctx.r6;
//     auto r7 = ctx.r7;
//     auto r8 = ctx.r8;

//     const char* arFilePath = reinterpret_cast<const char*>(base + PPC_LOAD_U32(r5.u32));

//     // __HH_ALLOC
//     ctx.r3.u32 = 22 + strlen(arFilePath);
//     sub_822C0988(ctx, base);
//     char* prefixedArFilePath = reinterpret_cast<char*>(base + ctx.r3.u32);

//     *reinterpret_cast<be<uint32_t>*>(prefixedArFilePath) = 1;
//     strcpy(prefixedArFilePath + 0x4, "/UnleashedRecomp/");
//     strcpy(prefixedArFilePath + 0x15, arFilePath);

//     ctx.r1.u32 -= 0x10;
//     uint32_t stackSpace = ctx.r1.u32;
//     PPC_STORE_U32(stackSpace, static_cast<uint32_t>(reinterpret_cast<uint8_t*>(prefixedArFilePath) - base) + 0x4);
//     g_prefixedArFilePath = stackSpace;

//     ctx.r3 = r3;
//     ctx.r4 = r4;
//     ctx.r5 = r5;
//     ctx.r6 = r6;
//     ctx.r7 = r7;
//     ctx.r8 = r8;
//     __imp__sub_82E0CC38(ctx, base);

//     // Hedgehog::Base::CSharedString::~CSharedString
//     ctx.r3.u32 = stackSpace;
//     sub_82DFB148(ctx, base);

//     g_prefixedArFilePath = NULL;
//     ctx.r1.u32 += 0x10;
// }

// Hedgehog::Database::SLoadElement::SLoadElement
// PPC_FUNC_IMPL(__imp__sub_82E140D8);
// PPC_FUNC(sub_82E140D8)
// {
//     // Store the prefixed archive file path as the pretty name. It's unused for archives we want to append to.
//     if (!g_mods.empty() && PPC_LOAD_U32(ctx.r5.u32) == 0x8200A621 && g_prefixedArFilePath != NULL)
//         ctx.r5.u32 = g_prefixedArFilePath;

//     __imp__sub_82E140D8(ctx, base);
// }

// Hedgehog::Database::CDatabaseLoader::CCreateFromArchive::CreateCallback
// PPC_FUNC_IMPL(__imp__sub_82E0B500);
// PPC_FUNC(sub_82E0B500)
// {
//     if (g_mods.empty())
//     {
//         __imp__sub_82E0B500(ctx, base);
//         return;
//     }

//     uint32_t prefixedArFilePath = PPC_LOAD_U32(ctx.r5.u32);
//     std::u8string_view arFilePathU8(reinterpret_cast<const char8_t*>(base + prefixedArFilePath));
//     if (!arFilePathU8.starts_with(u8"/UnleashedRecomp/"))
//     {
//         __imp__sub_82E0B500(ctx, base);
//         return;
//     }

//     // Immediately clear the string, so the remaining splits don't load append archives again.
//     PPC_STORE_U8(prefixedArFilePath, 0x00);
//     arFilePathU8.remove_prefix(0x11);

//     auto r3 = ctx.r3; // Callback
//     auto r4 = ctx.r4; // Database
//     auto r5 = ctx.r5; // Name
//     auto r6 = ctx.r6; // Data
//     auto r7 = ctx.r7; // Size
//     auto r8 = ctx.r8; // Callback data

//     auto loadArchive = [&](const std::filesystem::path& arFilePath)
//         {
//             std::ifstream stream(arFilePath, std::ios::binary);
//             if (stream.good())
//             {
//                 if (ModLoader::s_isLogTypeConsole)
//                     LOGF_IMPL(Utility, "Mod Loader", "Loading file: \"{}\"", reinterpret_cast<const char*>(arFilePath.u8string().c_str()));

//                 stream.seekg(0, std::ios::end);
//                 size_t arFileSize = stream.tellg();

//                 void* arFileData = g_userHeap.Alloc(arFileSize);
//                 stream.seekg(0, std::ios::beg);
//                 stream.read(reinterpret_cast<char*>(arFileData), arFileSize);
//                 stream.close();

//                 auto arFileDataHolder = reinterpret_cast<be<uint32_t>*>(g_userHeap.Alloc(sizeof(uint32_t) * 2));

//                 if (*reinterpret_cast<be<uint32_t>*>(arFileData) == LZX_SIGNATURE)
//                 {
//                     auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(arFileData), arFileSize, arFileDataHolder);

//                     g_userHeap.Free(arFileData);

//                     arFileData = fileData.data();
//                     arFileSize = fileData.size();
//                 }

//                 arFileDataHolder[0] = g_memory.MapVirtual(arFileData);
//                 arFileDataHolder[1] = NULL;

//                 ctx.r3 = r3;
//                 ctx.r4 = r4;
//                 ctx.r5 = r5;
//                 ctx.r6.u32 = g_memory.MapVirtual(arFileDataHolder);
//                 ctx.r7.u32 = uint32_t(arFileSize);
//                 ctx.r8 = r8;

//                 __imp__sub_82E0B500(ctx, base);

//                 g_userHeap.Free(arFileDataHolder);
//                 g_userHeap.Free(arFileData);

//                 return true;
//             }

//             return false;
//         };

//     thread_local xxHashMap<std::vector<std::filesystem::path>> s_cache;

//     XXH64_hash_t hash = XXH3_64bits(arFilePathU8.data(), arFilePathU8.size());
//     auto findResult = s_cache.find(hash);
//     if (findResult != s_cache.end())
//     {
//         for (const auto& arFilePath : findResult->second)
//             loadArchive(arFilePath);
//     }
//     else
//     {
//         std::vector<std::filesystem::path> arFilePaths;
//         std::filesystem::path arFilePath;
//         std::filesystem::path appendArFilePath;

//         for (auto& mod : g_mods)
//         {
//             for (auto& includeDir : mod.includeDirs)
//             {
//                 auto loadUncachedArchive = [&](const std::filesystem::path& arFilePath)
//                     {
//                         if (mod.type == ModType::UMM && mod.readOnly.contains(arFilePath))
//                             return false;

//                         std::filesystem::path combinedFilePath = includeDir / arFilePath;
//                         bool success = loadArchive(combinedFilePath);
//                         if (success)
//                             arFilePaths.emplace_back(std::move(combinedFilePath));

//                         return success;
//                     };

//                 auto loadArchives = [&](const std::filesystem::path& arFilePath)
//                     {
//                         thread_local std::filesystem::path s_tempPath;
//                         s_tempPath = arFilePath;
//                         s_tempPath += "l";

//                         if (mod.type == ModType::UMM && mod.readOnly.contains(s_tempPath))
//                             return;

//                         std::ifstream stream(includeDir / s_tempPath, std::ios::binary);
//                         if (stream.good())
//                         {
//                             be<uint32_t> signature{};
//                             uint32_t splitCount{};
//                             stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));

//                             if (signature == LZX_SIGNATURE)
//                             {
//                                 stream.seekg(0, std::ios::end);
//                                 size_t arlFileSize = stream.tellg();
//                                 stream.seekg(0, std::ios::beg);

//                                 void* compressedFileData = g_userHeap.Alloc(arlFileSize);
//                                 stream.read(reinterpret_cast<char*>(compressedFileData), arlFileSize);
//                                 stream.close();

//                                 auto fileData = decompressLzx(ctx, base, reinterpret_cast<uint8_t*>(compressedFileData), arlFileSize, nullptr);

//                                 g_userHeap.Free(compressedFileData);

//                                 splitCount = *reinterpret_cast<uint32_t*>(fileData.data() + 0x4);

//                                 g_userHeap.Free(fileData.data());
//                             }
//                             else
//                             {
//                                 stream.read(reinterpret_cast<char*>(&splitCount), sizeof(splitCount));
//                                 stream.close();
//                             }

//                             if (splitCount == 0)
//                             {
//                                 loadUncachedArchive(arFilePath);
//                             }
//                             else
//                             {
//                                 for (uint32_t i = 0; i < splitCount; i++)
//                                 {
//                                     s_tempPath = arFilePath;
//                                     s_tempPath += fmt::format(".{:02}", i);
//                                     loadUncachedArchive(s_tempPath);
//                                 }
//                             }
//                         }
//                         else if (mod.type == ModType::UMM)
//                         {
//                             if (!loadUncachedArchive(arFilePath))
//                             {
//                                 for (uint32_t i = 0; ; i++)
//                                 {
//                                     s_tempPath = arFilePath;
//                                     s_tempPath += fmt::format(".{:02}", i);
//                                     if (!loadUncachedArchive(s_tempPath))
//                                         break;
//                                 }
//                             }
//                         }
//                     };

//                 if (mod.type == ModType::UMM)
//                 {
//                     if (mod.merge)
//                     {
//                         if (arFilePath.empty())
//                             arFilePath = arFilePathU8;

//                         loadArchives(arFilePath);
//                     }
//                 }
//                 else if (mod.type == ModType::HMM)
//                 {
//                     if (appendArFilePath.empty())
//                     {
//                         if (arFilePath.empty())
//                             arFilePath = arFilePathU8;

//                         appendArFilePath = arFilePath.parent_path();
//                         appendArFilePath /= "+";
//                         appendArFilePath += arFilePath.filename();
//                     }

//                     loadArchives(appendArFilePath);
//                 }
//             }
//         }

//         s_cache.emplace(hash, std::move(arFilePaths));
//     }

//     ctx.r3 = r3;
//     ctx.r4 = r4;
//     ctx.r5 = r5;
//     ctx.r6 = r6;
//     ctx.r7 = r7;
//     ctx.r8 = r8;

//     __imp__sub_82E0B500(ctx, base);
// }

// CriAuObjLoc::AttachCueSheet
// PPC_FUNC_IMPL(__imp__sub_8314A310);
// PPC_FUNC(sub_8314A310)
// {
//     // allocator: 0x4
//     // capacity: 0x24
//     // count: 0x28
//     // data: 0x2C
//     uint32_t capacity = PPC_LOAD_U32(ctx.r3.u32 + 0x24);
//     if (capacity == PPC_LOAD_U32(ctx.r3.u32 + 0x28))
//     {
//         auto r3 = ctx.r3;
//         auto r4 = ctx.r4;
//         auto r5 = ctx.r5;

//         // Allocate
//         ctx.r3.u32 = PPC_LOAD_U32(r3.u32 + 0x4);
//         ctx.r4.u32 = (capacity * 2) * sizeof(uint32_t);
//         ctx.r5.u32 = 0x82195248; // AuObjCueSheet
//         ctx.r6.u32 = 0x4;
//         sub_83167FD8(ctx, base);

//         // Copy
//         uint32_t oldData = PPC_LOAD_U32(r3.u32 + 0x2C);
//         uint32_t newData = ctx.r3.u32;

//         memcpy(base + newData, base + oldData, capacity * sizeof(uint32_t));
//         memset(base + newData + (capacity * sizeof(uint32_t)), 0, capacity * sizeof(uint32_t));

//         PPC_STORE_U32(r3.u32 + 0x24, capacity * 2);
//         PPC_STORE_U32(r3.u32 + 0x2C, newData);

//         // Deallocate
//         ctx.r3.u32 = PPC_LOAD_U32(r3.u32 + 0x4);
//         ctx.r4.u32 = oldData;
//         sub_83168100(ctx, base);

//         ctx.r3 = r3;
//         ctx.r4 = r4;
//         ctx.r5 = r5;
//     }

//     __imp__sub_8314A310(ctx, base);
// }
