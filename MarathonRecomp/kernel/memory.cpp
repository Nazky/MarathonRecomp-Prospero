#include <stdafx.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <os/logger.h>
#if defined(__PROSPERO__)
#include <ps5platform/shm.h>
#endif
#include "memory.h"

#if defined(__PROSPERO__)
namespace
{
    ps5_shm g_guestMemoryBacking{};

    void LogMemoryDiagf(const char* format, ...)
    {
        char buffer[256];
        va_list args;
        va_start(args, format);
        const int result = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (result > 0)
        {
            const size_t length = static_cast<size_t>(result) < sizeof(buffer)
                ? static_cast<size_t>(result) : sizeof(buffer) - 1;
            os::logger::Log(std::string_view(buffer, length), os::logger::ELogType::Warning, "PS5-DIAG");
        }
    }
}
#endif

Memory::Memory()
{
#if defined(__PROSPERO__)
    LOG_PS5_DIAG("Memory::Memory begin");
#endif
#ifdef _WIN32
    base = (uint8_t*)VirtualAlloc((void*)0x100000000ull, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        base = (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        return;

    DWORD oldProtect;
    VirtualProtect(base, 4096, PAGE_NOACCESS, &oldProtect);
#elif defined(__PROSPERO__)
    // The 4 GiB MAP_ANON request fails with ENOMEM on the console's flexible
    // pool; back the guest arena with the SDK's direct-memory shared mapping.
    const int createResult = ps5_shm_create(PPC_MEMORY_SIZE, &g_guestMemoryBacking);
    LogMemoryDiagf("Memory::Memory direct-memory create result=0x%08x start=0x%llx bytes=0x%llx",
                   static_cast<unsigned int>(createResult),
                   static_cast<unsigned long long>(g_guestMemoryBacking.direct_start),
                   static_cast<unsigned long long>(g_guestMemoryBacking.bytes));
    if (createResult != 0)
    {
        base = nullptr;
        return;
    }

    void* guestMemoryView = nullptr;
    const int mapResult = ps5_shm_map(
        &g_guestMemoryBacking, 0, PPC_MEMORY_SIZE,
        reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000000000ull)),
        PS5_SHM_READ | PS5_SHM_WRITE, 0, &guestMemoryView);
    LogMemoryDiagf("Memory::Memory direct-memory map result=0x%08x base=0x%llx",
                   static_cast<unsigned int>(mapResult),
                   static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(guestMemoryView)));
    if (mapResult != 0 || guestMemoryView == nullptr)
    {
        ps5_shm_destroy(&g_guestMemoryBacking);
        base = nullptr;
        return;
    }

    base = static_cast<uint8_t*>(guestMemoryView);
    const int protectionResult = mprotect(base, 4096, PROT_NONE);
    const int protectionError = protectionResult == 0 ? 0 : errno;
    LogMemoryDiagf("Memory::Memory first-page mprotect result=%d errno=%d",
                   protectionResult, protectionError);
    LOG_PS5_DIAG("Memory::Memory function-mapping table initialization begin");
    (void)protectionResult;
#else
    base = (uint8_t*)mmap((void*)0x100000000ull, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
        base = (uint8_t*)mmap(NULL, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == nullptr || base == (uint8_t*)MAP_FAILED)
        return;

    const int protectionResult = mprotect(base, 4096, PROT_NONE);
    (void)protectionResult;
#endif

#if defined(__PROSPERO__)
    size_t insertedFunctionMappings = 0;
#endif
    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
        {
            InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
#if defined(__PROSPERO__)
            ++insertedFunctionMappings;
#endif
        }
    }
#if defined(__PROSPERO__)
    LogMemoryDiagf("Memory::Memory complete base=0x%llx inserted_function_mappings=%zu",
                   static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(base)),
                   insertedFunctionMappings);
#endif
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
