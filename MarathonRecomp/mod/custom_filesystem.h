#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace CustomFilesystem
{
struct ArchiveFile
{
    std::string name;
    std::string path;
};

struct ArchiveCategory
{
    std::string name;
    std::vector<ArchiveFile> files;
};

struct ArchivePackage
{
    bool bigEndian = true;
    std::vector<ArchiveCategory> categories;
};

// Reads and parses the platform's archive.pkg from the system.arc U8 archive.
bool ReadArchivePackage(
    const std::filesystem::path& systemArchivePath,
    std::string_view platformDirectory,
    ArchivePackage& package,
    std::string& errorMessage);

// Writes a user-data copy of system.arc with the supplied archive.pkg entries.
// The packaged game archive is only read; it is never modified.
bool WriteSystemArchiveOverlay(
    const std::filesystem::path& systemArchivePath,
    const std::filesystem::path& outputPath,
    std::string_view platformDirectory,
    const ArchivePackage& package,
    std::string& errorMessage);

bool HasArchivePackageEntry(
    const ArchivePackage& package,
    std::string_view categoryName,
    std::string_view archiveName,
    std::string_view archivePath);
}
