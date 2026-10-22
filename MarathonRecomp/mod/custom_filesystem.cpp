#include "custom_filesystem.h"

#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace CustomFilesystem
{
namespace
{
constexpr uint32_t U8_SIGNATURE = 0x55AA382D;
constexpr size_t U8_HEADER_SIZE = 0x20;
constexpr size_t U8_NODE_SIZE = 0x10;
constexpr size_t BINA_HEADER_SIZE = 0x20;
constexpr size_t MAX_SYSTEM_ARCHIVE_SIZE = size_t(1024) * 1024 * 1024;
constexpr size_t MAX_PACKAGE_SIZE = size_t(128) * 1024 * 1024;
constexpr uint32_t MAX_U8_NODE_COUNT = 1'000'000;
constexpr uint32_t MAX_PACKAGE_CATEGORY_COUNT = 4096;
constexpr uint32_t MAX_PACKAGE_FILE_COUNT = 250'000;

struct U8Node
{
    uint8_t type{};
    uint32_t fieldA{};
    uint32_t length{};
    uint32_t uncompressedSize{};
    size_t recordOffset{};
    std::string name;
    std::string fullPath;
};

static uint32_t ReadBigEndian32(const std::vector<uint8_t>& data, size_t offset)
{
    return (uint32_t(data[offset]) << 24) |
        (uint32_t(data[offset + 1]) << 16) |
        (uint32_t(data[offset + 2]) << 8) |
        uint32_t(data[offset + 3]);
}

static uint32_t Read32(const std::vector<uint8_t>& data, size_t offset, bool bigEndian)
{
    if (bigEndian)
        return ReadBigEndian32(data, offset);

    return uint32_t(data[offset]) |
        (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) |
        (uint32_t(data[offset + 3]) << 24);
}

static bool HasRange(size_t size, size_t offset, size_t length)
{
    return offset <= size && length <= size - offset;
}

static bool ReadNullTerminatedString(
    const std::vector<uint8_t>& data,
    size_t offset,
    std::string& value,
    size_t maximumLength = 4096)
{
    if (offset >= data.size())
        return false;

    size_t end = offset;
    while (end < data.size() && end - offset <= maximumLength && data[end] != 0)
        ++end;
    if (end >= data.size() || end - offset > maximumLength)
        return false;

    value.assign(reinterpret_cast<const char*>(data.data() + offset), end - offset);
    return true;
}

static std::string LowerAscii(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

static std::string NormalizeArchivePath(std::string_view value)
{
    std::string result(value);
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.front() == '/')
        result.erase(result.begin());
    return LowerAscii(result);
}

static bool ReadWholeFile(
    const std::filesystem::path& path,
    std::vector<uint8_t>& data,
    std::string& errorMessage,
    size_t maximumSize)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        errorMessage = "could not open " + path.string();
        return false;
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff end = stream.tellg();
    if (end < 0 || static_cast<uint64_t>(end) > maximumSize)
    {
        errorMessage = "file size is invalid or exceeds the supported limit: " + path.string();
        return false;
    }

    data.resize(static_cast<size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!data.empty())
        stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!stream || static_cast<size_t>(stream.gcount()) != data.size())
    {
        errorMessage = "could not read the complete file: " + path.string();
        return false;
    }

    return true;
}

static bool ParseU8Archive(
    const std::vector<uint8_t>& data,
    std::vector<U8Node>& nodes,
    std::string& errorMessage)
{
    if (data.size() < U8_HEADER_SIZE || ReadBigEndian32(data, 0) != U8_SIGNATURE)
    {
        errorMessage = "system.arc does not have a supported U8 archive header";
        return false;
    }

    const uint32_t entriesOffset = ReadBigEndian32(data, 4);
    const uint32_t dataOffset = ReadBigEndian32(data, 12);
    if (!HasRange(data.size(), entriesOffset, U8_NODE_SIZE))
    {
        errorMessage = "system.arc has an invalid U8 node-table offset";
        return false;
    }

    const uint32_t nodeCount = ReadBigEndian32(data, size_t(entriesOffset) + 8);
    if (nodeCount == 0 || nodeCount > MAX_U8_NODE_COUNT)
    {
        errorMessage = "system.arc has an invalid U8 node count";
        return false;
    }

    const uint64_t nodeTableLength = uint64_t(nodeCount) * U8_NODE_SIZE;
    if (nodeTableLength > std::numeric_limits<size_t>::max() ||
        !HasRange(data.size(), entriesOffset, static_cast<size_t>(nodeTableLength)))
    {
        errorMessage = "system.arc has a truncated U8 node table";
        return false;
    }

    const size_t stringTableOffset = size_t(entriesOffset) + static_cast<size_t>(nodeTableLength);
    if (stringTableOffset > data.size() || dataOffset > data.size())
    {
        errorMessage = "system.arc has an invalid U8 data or string-table offset";
        return false;
    }

    nodes.clear();
    nodes.reserve(nodeCount);

    for (uint32_t index = 0; index < nodeCount; ++index)
    {
        const size_t recordOffset = size_t(entriesOffset) + size_t(index) * U8_NODE_SIZE;
        const uint32_t flags = ReadBigEndian32(data, recordOffset);
        const uint32_t nameOffset = flags & 0x00FFFFFF;
        const uint32_t fieldA = ReadBigEndian32(data, recordOffset + 4);
        const uint32_t length = ReadBigEndian32(data, recordOffset + 8);
        const uint32_t uncompressedSize = ReadBigEndian32(data, recordOffset + 12);
        const uint8_t type = static_cast<uint8_t>(flags >> 24);

        if (type > 1)
        {
            errorMessage = "system.arc contains an unsupported U8 node type";
            return false;
        }

        U8Node node;
        node.type = type;
        node.fieldA = fieldA;
        node.length = length;
        node.uncompressedSize = uncompressedSize;
        node.recordOffset = recordOffset;
        if (!ReadNullTerminatedString(data, stringTableOffset + nameOffset, node.name))
        {
            errorMessage = "system.arc contains an invalid U8 node name";
            return false;
        }
        nodes.emplace_back(std::move(node));
    }

    if (nodes.front().type != 1 || nodes.front().length != nodeCount)
    {
        errorMessage = "system.arc has an invalid U8 root directory";
        return false;
    }

    nodes.front().fullPath.clear();
    std::vector<std::pair<uint32_t, std::string>> directoryStack;
    directoryStack.emplace_back(nodeCount, std::string());

    for (uint32_t index = 1; index < nodeCount; ++index)
    {
        while (directoryStack.size() > 1 && index >= directoryStack.back().first)
            directoryStack.pop_back();
        if (directoryStack.empty())
        {
            errorMessage = "system.arc contains an invalid U8 directory hierarchy";
            return false;
        }

        U8Node& node = nodes[index];
        node.fullPath = directoryStack.back().second;
        if (!node.fullPath.empty())
            node.fullPath.push_back('/');
        node.fullPath += node.name;

        if (node.type == 1)
        {
            if (node.length <= index || node.length > nodeCount)
            {
                errorMessage = "system.arc contains an invalid U8 directory range";
                return false;
            }
            if (node.length > index + 1)
                directoryStack.emplace_back(node.length, node.fullPath);
        }
        else
        {
            if (!HasRange(data.size(), node.fieldA, node.length))
            {
                errorMessage = "system.arc contains an invalid U8 file range";
                return false;
            }
            if (node.uncompressedSize != 0 && node.fieldA < dataOffset)
            {
                errorMessage = "system.arc contains a compressed file outside its data section";
                return false;
            }
        }
    }

    return true;
}

static int FindArchivePackageNode(const std::vector<U8Node>& nodes, std::string_view platformDirectory)
{
    std::string expected(platformDirectory);
    std::replace(expected.begin(), expected.end(), '\\', '/');
    while (!expected.empty() && expected.front() == '/')
        expected.erase(expected.begin());
    if (!expected.empty())
        expected.push_back('/');
    expected += "archive.pkg";
    expected = LowerAscii(expected);

    int fallback = -1;
    for (size_t i = 1; i < nodes.size(); ++i)
    {
        if (nodes[i].type != 0)
            continue;
        const std::string fullPath = LowerAscii(nodes[i].fullPath);
        if (fullPath == expected)
            return static_cast<int>(i);
        if (nodes[i].name == "archive.pkg" || LowerAscii(nodes[i].name) == "archive.pkg")
        {
            if (fallback >= 0)
                return -1; // Ambiguous fallback: require an exact platform path.
            fallback = static_cast<int>(i);
        }
    }
    return fallback;
}

static bool ReadPackageBytes(
    const std::vector<uint8_t>& archiveData,
    const U8Node& node,
    std::vector<uint8_t>& packageData,
    std::string& errorMessage)
{
    if (!HasRange(archiveData.size(), node.fieldA, node.length))
    {
        errorMessage = "archive.pkg data range is outside system.arc";
        return false;
    }

    const uint8_t* source = archiveData.data() + node.fieldA;
    if (node.uncompressedSize == 0)
    {
        packageData.assign(source, source + node.length);
        return true;
    }

    if (node.uncompressedSize > MAX_PACKAGE_SIZE || node.length > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        node.uncompressedSize > static_cast<uint32_t>(std::numeric_limits<int>::max()))
    {
        errorMessage = "compressed archive.pkg exceeds the supported size";
        return false;
    }

    packageData.resize(node.uncompressedSize);
    const int decodedSize = stbi_zlib_decode_buffer(
        reinterpret_cast<char*>(packageData.data()),
        static_cast<int>(packageData.size()),
        reinterpret_cast<const char*>(source),
        static_cast<int>(node.length));
    if (decodedSize < 0 || static_cast<size_t>(decodedSize) != packageData.size())
    {
        errorMessage = "could not decompress archive.pkg from system.arc";
        return false;
    }

    return true;
}

static void Write16At(std::vector<uint8_t>& data, size_t offset, uint16_t value, bool bigEndian)
{
    if (bigEndian)
    {
        data[offset] = static_cast<uint8_t>(value >> 8);
        data[offset + 1] = static_cast<uint8_t>(value);
    }
    else
    {
        data[offset] = static_cast<uint8_t>(value);
        data[offset + 1] = static_cast<uint8_t>(value >> 8);
    }
}

static void Write32At(std::vector<uint8_t>& data, size_t offset, uint32_t value, bool bigEndian)
{
    if (bigEndian)
    {
        data[offset] = static_cast<uint8_t>(value >> 24);
        data[offset + 1] = static_cast<uint8_t>(value >> 16);
        data[offset + 2] = static_cast<uint8_t>(value >> 8);
        data[offset + 3] = static_cast<uint8_t>(value);
    }
    else
    {
        data[offset] = static_cast<uint8_t>(value);
        data[offset + 1] = static_cast<uint8_t>(value >> 8);
        data[offset + 2] = static_cast<uint8_t>(value >> 16);
        data[offset + 3] = static_cast<uint8_t>(value >> 24);
    }
}

static bool AddSize(std::vector<uint8_t>& data, size_t count, std::string& errorMessage)
{
    if (count > std::numeric_limits<uint32_t>::max() || data.size() > std::numeric_limits<uint32_t>::max() - count)
    {
        errorMessage = "archive.pkg exceeds the supported 32-bit size";
        return false;
    }
    data.resize(data.size() + count, 0);
    return true;
}

struct PendingString
{
    size_t pointerOffset{};
    std::string value;
};

static bool SerializeArchivePackage(
    const ArchivePackage& package,
    std::vector<uint8_t>& data,
    std::string& errorMessage)
{
    if (package.categories.size() > MAX_PACKAGE_CATEGORY_COUNT)
    {
        errorMessage = "archive.pkg has too many categories";
        return false;
    }

    uint64_t totalFiles = 0;
    for (const auto& category : package.categories)
        totalFiles += category.files.size();
    if (totalFiles > MAX_PACKAGE_FILE_COUNT)
    {
        errorMessage = "archive.pkg has too many archive entries";
        return false;
    }

    data.assign(BINA_HEADER_SIZE, 0);
    std::vector<size_t> offsetFields;
    std::vector<PendingString> pendingStrings;

    auto append32 = [&](uint32_t value)
    {
        const size_t offset = data.size();
        data.resize(offset + 4);
        Write32At(data, offset, value, package.bigEndian);
    };
    auto reserveOffset = [&]()
    {
        const size_t offset = data.size();
        offsetFields.emplace_back(offset);
        append32(0);
        return offset;
    };
    auto addString = [&](const std::string& value)
    {
        if (value.empty())
        {
            append32(0);
            return;
        }
        const size_t pointerOffset = reserveOffset();
        pendingStrings.push_back({ pointerOffset, value });
    };
    auto patchRelativeOffset = [&](size_t pointerOffset, size_t targetOffset)
    {
        if (targetOffset < BINA_HEADER_SIZE || targetOffset - BINA_HEADER_SIZE > std::numeric_limits<uint32_t>::max())
            return false;
        Write32At(data, pointerOffset, static_cast<uint32_t>(targetOffset - BINA_HEADER_SIZE), package.bigEndian);
        return true;
    };

    append32(static_cast<uint32_t>(totalFiles));
    const size_t fileEntriesPointer = reserveOffset();
    append32(static_cast<uint32_t>(package.categories.size()));
    const size_t categoryEntriesPointer = reserveOffset();

    const size_t categoryEntriesStart = data.size();
    if (!patchRelativeOffset(categoryEntriesPointer, categoryEntriesStart))
    {
        errorMessage = "archive.pkg category table offset is invalid";
        return false;
    }

    struct CategoryOutputOffsets
    {
        size_t fileTablePointer{};
    };
    std::vector<CategoryOutputOffsets> categoryOffsets;
    categoryOffsets.reserve(package.categories.size());

    for (const auto& category : package.categories)
    {
        addString(category.name);
        append32(static_cast<uint32_t>(category.files.size()));
        categoryOffsets.push_back({ reserveOffset() });
    }

    const size_t fileEntriesStart = data.size();
    if (!patchRelativeOffset(fileEntriesPointer, fileEntriesStart))
    {
        errorMessage = "archive.pkg file table offset is invalid";
        return false;
    }

    for (size_t categoryIndex = 0; categoryIndex < package.categories.size(); ++categoryIndex)
    {
        if (!patchRelativeOffset(categoryOffsets[categoryIndex].fileTablePointer, data.size()))
        {
            errorMessage = "archive.pkg category file offset is invalid";
            return false;
        }

        for (const auto& file : package.categories[categoryIndex].files)
        {
            addString(file.name);
            addString(file.path);
        }
    }

    if (!AddSize(data, (4 - (data.size() & 3)) & 3, errorMessage))
        return false;

    for (const auto& pending : pendingStrings)
    {
        if (!patchRelativeOffset(pending.pointerOffset, data.size()))
        {
            errorMessage = "archive.pkg string offset is invalid";
            return false;
        }
        if (pending.value.find('\0') != std::string::npos || data.size() >= MAX_PACKAGE_SIZE ||
            pending.value.size() >= MAX_PACKAGE_SIZE - data.size())
        {
            errorMessage = "archive.pkg contains an invalid or oversized string";
            return false;
        }
        data.insert(data.end(), pending.value.begin(), pending.value.end());
        data.push_back(0);
    }

    if (!AddSize(data, (4 - (data.size() & 3)) & 3, errorMessage))
        return false;

    const size_t footerStart = data.size();
    size_t lastOffset = BINA_HEADER_SIZE;
    for (const size_t offset : offsetFields)
    {
        if (offset < lastOffset || ((offset - lastOffset) & 3) != 0)
        {
            errorMessage = "archive.pkg contains an invalid BINA offset table";
            return false;
        }

        const uint32_t delta = static_cast<uint32_t>((offset - lastOffset) >> 2);
        if (delta <= 0x3F)
        {
            data.push_back(static_cast<uint8_t>(0x40 | delta));
        }
        else if (delta <= 0x3FFF)
        {
            const uint16_t encoded = static_cast<uint16_t>(0x8000 | delta);
            data.push_back(static_cast<uint8_t>(encoded >> 8));
            data.push_back(static_cast<uint8_t>(encoded));
        }
        else if (delta <= 0x3FFFFFFF)
        {
            const uint32_t encoded = 0xC0000000 | delta;
            data.push_back(static_cast<uint8_t>(encoded >> 24));
            data.push_back(static_cast<uint8_t>(encoded >> 16));
            data.push_back(static_cast<uint8_t>(encoded >> 8));
            data.push_back(static_cast<uint8_t>(encoded));
        }
        else
        {
            errorMessage = "archive.pkg BINA offset exceeds the supported range";
            return false;
        }
        lastOffset = offset;
    }

    if (!AddSize(data, (4 - (data.size() & 3)) & 3, errorMessage) ||
        data.size() > std::numeric_limits<uint32_t>::max())
    {
        if (errorMessage.empty())
            errorMessage = "archive.pkg exceeds the supported 32-bit size";
        return false;
    }

    const uint32_t fileSize = static_cast<uint32_t>(data.size());
    const uint32_t finalTableOffset = static_cast<uint32_t>(footerStart - BINA_HEADER_SIZE);
    const uint32_t finalTableLength = static_cast<uint32_t>(data.size() - footerStart);
    Write32At(data, 0, fileSize, package.bigEndian);
    Write32At(data, 4, finalTableOffset, package.bigEndian);
    Write32At(data, 8, finalTableLength, package.bigEndian);
    Write32At(data, 12, 0, package.bigEndian);
    Write16At(data, 16, 0, package.bigEndian);
    Write16At(data, 18, 0, package.bigEndian); // No BINA footer magic.
    data[20] = 0;
    data[21] = 0;
    data[22] = static_cast<uint8_t>('1');
    data[23] = package.bigEndian ? static_cast<uint8_t>('B') : static_cast<uint8_t>('L');
    data[24] = 'B';
    data[25] = 'I';
    data[26] = 'N';
    data[27] = 'A';
    return true;
}

static bool ParseArchivePackage(
    const std::vector<uint8_t>& data,
    ArchivePackage& package,
    std::string& errorMessage)
{
    if (data.size() < BINA_HEADER_SIZE + 0x10 ||
        std::memcmp(data.data() + 0x18, "BINA", 4) != 0)
    {
        errorMessage = "archive.pkg does not have a supported BINA header";
        return false;
    }

    if (data[0x17] == 'B')
        package.bigEndian = true;
    else if (data[0x17] == 'L')
        package.bigEndian = false;
    else
    {
        errorMessage = "archive.pkg has an unknown BINA byte order";
        return false;
    }

    const uint32_t fileCount = Read32(data, BINA_HEADER_SIZE, package.bigEndian);
    const uint32_t fileEntriesRelative = Read32(data, BINA_HEADER_SIZE + 4, package.bigEndian);
    const uint32_t categoryCount = Read32(data, BINA_HEADER_SIZE + 8, package.bigEndian);
    const uint32_t categoryEntriesRelative = Read32(data, BINA_HEADER_SIZE + 12, package.bigEndian);

    if (categoryCount > MAX_PACKAGE_CATEGORY_COUNT || fileCount > MAX_PACKAGE_FILE_COUNT)
    {
        errorMessage = "archive.pkg has too many categories or files";
        return false;
    }

    const uint64_t categoryEntriesOffset64 = uint64_t(BINA_HEADER_SIZE) + categoryEntriesRelative;
    const uint64_t fileEntriesOffset64 = uint64_t(BINA_HEADER_SIZE) + fileEntriesRelative;
    if (categoryEntriesOffset64 > data.size() || fileEntriesOffset64 > data.size() ||
        uint64_t(categoryCount) * 12 > data.size() - static_cast<size_t>(categoryEntriesOffset64))
    {
        errorMessage = "archive.pkg has an invalid category or file table offset";
        return false;
    }

    package.categories.clear();
    package.categories.reserve(categoryCount);
    uint64_t parsedFileCount = 0;

    for (uint32_t categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex)
    {
        const size_t recordOffset = static_cast<size_t>(categoryEntriesOffset64) + size_t(categoryIndex) * 12;
        const uint32_t nameRelative = Read32(data, recordOffset, package.bigEndian);
        const uint32_t categoryFileCount = Read32(data, recordOffset + 4, package.bigEndian);
        const uint32_t categoryFilesRelative = Read32(data, recordOffset + 8, package.bigEndian);
        const uint64_t nameOffset64 = uint64_t(BINA_HEADER_SIZE) + nameRelative;
        const uint64_t filesOffset64 = uint64_t(BINA_HEADER_SIZE) + categoryFilesRelative;

        if (categoryFileCount > MAX_PACKAGE_FILE_COUNT ||
            filesOffset64 > data.size() || uint64_t(categoryFileCount) * 8 > data.size() - static_cast<size_t>(filesOffset64) ||
            nameOffset64 >= data.size())
        {
            errorMessage = "archive.pkg has an invalid category entry";
            return false;
        }

        ArchiveCategory category;
        if (!ReadNullTerminatedString(data, static_cast<size_t>(nameOffset64), category.name))
        {
            errorMessage = "archive.pkg has an invalid category name";
            return false;
        }

        category.files.reserve(categoryFileCount);
        for (uint32_t fileIndex = 0; fileIndex < categoryFileCount; ++fileIndex)
        {
            const size_t fileRecordOffset = static_cast<size_t>(filesOffset64) + size_t(fileIndex) * 8;
            const uint32_t fileNameRelative = Read32(data, fileRecordOffset, package.bigEndian);
            const uint32_t filePathRelative = Read32(data, fileRecordOffset + 4, package.bigEndian);
            const uint64_t fileNameOffset64 = uint64_t(BINA_HEADER_SIZE) + fileNameRelative;
            const uint64_t filePathOffset64 = uint64_t(BINA_HEADER_SIZE) + filePathRelative;
            if (fileNameOffset64 >= data.size() || filePathOffset64 >= data.size())
            {
                errorMessage = "archive.pkg has an invalid archive-name or path offset";
                return false;
            }

            ArchiveFile file;
            if (!ReadNullTerminatedString(data, static_cast<size_t>(fileNameOffset64), file.name) ||
                !ReadNullTerminatedString(data, static_cast<size_t>(filePathOffset64), file.path))
            {
                errorMessage = "archive.pkg has an invalid archive name or path";
                return false;
            }
            category.files.emplace_back(std::move(file));
        }

        parsedFileCount += categoryFileCount;
        package.categories.emplace_back(std::move(category));
    }

    if (parsedFileCount > MAX_PACKAGE_FILE_COUNT || (fileCount != 0 && parsedFileCount != fileCount))
    {
        errorMessage = "archive.pkg file count does not match its category tables";
        return false;
    }

    return true;
}

static bool LoadArchivePackage(
    const std::filesystem::path& systemArchivePath,
    std::string_view platformDirectory,
    std::vector<uint8_t>& archiveData,
    std::vector<U8Node>& nodes,
    size_t& packageNodeIndex,
    ArchivePackage& package,
    std::string& errorMessage)
{
    if (!ReadWholeFile(systemArchivePath, archiveData, errorMessage, MAX_SYSTEM_ARCHIVE_SIZE))
        return false;
    if (!ParseU8Archive(archiveData, nodes, errorMessage))
        return false;

    const int nodeIndex = FindArchivePackageNode(nodes, platformDirectory);
    if (nodeIndex < 0)
    {
        errorMessage = "system.arc does not contain the requested platform archive.pkg";
        return false;
    }
    packageNodeIndex = static_cast<size_t>(nodeIndex);

    std::vector<uint8_t> packageData;
    if (!ReadPackageBytes(archiveData, nodes[packageNodeIndex], packageData, errorMessage))
        return false;
    if (!ParseArchivePackage(packageData, package, errorMessage))
        return false;

    return true;
}

} // namespace

bool ReadArchivePackage(
    const std::filesystem::path& systemArchivePath,
    std::string_view platformDirectory,
    ArchivePackage& package,
    std::string& errorMessage)
{
    std::vector<uint8_t> archiveData;
    std::vector<U8Node> nodes;
    size_t packageNodeIndex{};
    return LoadArchivePackage(systemArchivePath, platformDirectory, archiveData, nodes, packageNodeIndex, package, errorMessage);
}

bool HasArchivePackageEntry(
    const ArchivePackage& package,
    std::string_view categoryName,
    std::string_view archiveName,
    std::string_view archivePath)
{
    const std::string requestedCategory = LowerAscii(categoryName);
    const std::string requestedName = LowerAscii(archiveName);
    const std::string requestedPath = NormalizeArchivePath(archivePath);

    const auto category = std::find_if(package.categories.begin(), package.categories.end(), [&](const ArchiveCategory& value)
    {
        return LowerAscii(value.name) == requestedCategory;
    });
    if (category == package.categories.end())
        return false;

    return std::any_of(category->files.begin(), category->files.end(), [&](const ArchiveFile& file)
    {
        return LowerAscii(file.name) == requestedName || NormalizeArchivePath(file.path) == requestedPath;
    });
}

bool WriteSystemArchiveOverlay(
    const std::filesystem::path& systemArchivePath,
    const std::filesystem::path& outputPath,
    std::string_view platformDirectory,
    const ArchivePackage& package,
    std::string& errorMessage)
{
    std::vector<uint8_t> archiveData;
    std::vector<U8Node> nodes;
    size_t packageNodeIndex{};
    ArchivePackage originalPackage;
    if (!LoadArchivePackage(systemArchivePath, platformDirectory, archiveData, nodes, packageNodeIndex, originalPackage, errorMessage))
        return false;

    std::vector<uint8_t> packageData;
    if (!SerializeArchivePackage(package, packageData, errorMessage))
        return false;

    const uint64_t alignedOffset64 = (uint64_t(archiveData.size()) + 0x1F) & ~uint64_t(0x1F);
    if (alignedOffset64 > std::numeric_limits<uint32_t>::max() ||
        packageData.size() > std::numeric_limits<uint32_t>::max() - alignedOffset64)
    {
        errorMessage = "updated system.arc exceeds the supported 32-bit U8 file size";
        return false;
    }

    const size_t alignedOffset = static_cast<size_t>(alignedOffset64);
    archiveData.resize(alignedOffset, 0);
    archiveData.insert(archiveData.end(), packageData.begin(), packageData.end());

    const U8Node& packageNode = nodes[packageNodeIndex];
    Write32At(archiveData, packageNode.recordOffset + 4, static_cast<uint32_t>(alignedOffset), true);
    Write32At(archiveData, packageNode.recordOffset + 8, static_cast<uint32_t>(packageData.size()), true);
    Write32At(archiveData, packageNode.recordOffset + 12, 0, true); // Store the replacement package uncompressed.

    std::error_code filesystemError;
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path(), filesystemError);
        if (filesystemError)
        {
            errorMessage = "could not create the custom-filesystem cache directory: " + filesystemError.message();
            return false;
        }
    }

    std::filesystem::path temporaryPath = outputPath;
    temporaryPath += ".tmp";
    std::filesystem::remove(temporaryPath, filesystemError);
    filesystemError.clear();

    {
        std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            errorMessage = "could not open the custom-filesystem cache file for writing";
            return false;
        }
        stream.write(reinterpret_cast<const char*>(archiveData.data()), static_cast<std::streamsize>(archiveData.size()));
        stream.flush();
        if (!stream)
        {
            errorMessage = "could not write the complete custom-filesystem cache archive";
            stream.close();
            std::filesystem::remove(temporaryPath, filesystemError);
            return false;
        }
    }

    std::filesystem::remove(outputPath, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporaryPath, outputPath, filesystemError);
    if (filesystemError)
    {
        errorMessage = "could not publish the custom-filesystem cache archive: " + filesystemError.message();
        std::filesystem::remove(temporaryPath, filesystemError);
        return false;
    }

    return true;
}

} // namespace CustomFilesystem
