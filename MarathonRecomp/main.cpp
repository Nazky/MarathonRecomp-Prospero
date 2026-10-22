#include <cstdio>
#include <cerrno>
#include <stdafx.h>
#ifdef __x86_64__
#include <cpuid.h>
#endif
#include <cpu/guest_thread.h>
#include <gpu/video.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <file.h>
#include <vector>
#include <image.h>
#include <apu/audio.h>
#include <hid/hid.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/registry.h>
#include <kernel/xdbf.h>
#include <install/installer.h>
#include <install/update_checker.h>
#include <os/logger.h>
#include <os/process.h>
#include <os/registry.h>
#include <ui/game_window.h>
#include <ui/installer_wizard.h>
#include <mod/mod_loader.h>
#include <preload_executable.h>
#include <iostream>
#include <app.h>

#ifdef _WIN32
#include <timeapi.h>
#endif

#if defined(__PROSPERO__)
#include <sys/stat.h>
#include <unistd.h>
extern "C" int sceSystemServiceLoadExec(const char* path, char* const argv[]);
extern "C" int sceSystemServiceHideSplashScreen();
extern "C" int ps5_ffs(int value)
{
    return __builtin_ffs(value);
}
extern "C" time_t ps5_timegm(struct tm* value)
{
    return mktime(value);
}
extern "C" void catchReturnFromMain(int status)
{
    (void)status;
    (void)sceSystemServiceLoadExec("exit", nullptr);
    for (;;)
        usleep(100000);
}
[[noreturn]] static void MarathonExit(int status)
{
    catchReturnFromMain(status);
    __builtin_unreachable();
}
#else
[[noreturn]] static void MarathonExit(int status)
{
    std::_Exit(status);
}
#endif

#if defined(_WIN32) && defined(MARATHON_RECOMP_D3D12)
static std::array<std::string_view, 3> g_D3D12RequiredModules =
{
    "D3D12/D3D12Core.dll",
    "dxcompiler.dll",
    "dxil.dll"
};
#endif

const size_t XMAIOBegin = 0x7FEA0000;
const size_t XMAIOEnd = XMAIOBegin + 0x0000FFFF;

Memory g_memory;
Heap g_userHeap;
XDBFWrapper g_xdbfWrapper;
std::unordered_map<uint16_t, GuestTexture*> g_xdbfTextureCache;

void HostStartup()
{
    LOG_PS5_DIAG("HostStartup entered");
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    LOG_PS5_DIAG("HostStartup: hid::Init begin");
    hid::Init();
    LOG_PS5_DIAG("HostStartup: hid::Init end");
}

// Name inspired from nt's entry point
void KiSystemStartup()
{
    LOGF_PS5_DIAG("KiSystemStartup entered guest_memory_base=0x{:x}", reinterpret_cast<uintptr_t>(g_memory.base));
    if (g_memory.base == nullptr)
    {
        LOGN_ERROR("KiSystemStartup aborted: guest memory base is null.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
        MarathonExit(1);
    }

    LOG_PS5_DIAG("KiSystemStartup: guest heap initialization begin");
    g_userHeap.Init();
    LOG_PS5_DIAG("KiSystemStartup: guest heap initialization end");

    const auto gameContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Game");
    const std::string gamePath = (const char*)(GetGamePath() / "game").u8string().c_str();
    LOGF_PS5_DIAG("KiSystemStartup: game content created; game_path={}", gamePath);

    LOG_PS5_DIAG("KiSystemStartup: BuildPathCache begin");
    BuildPathCache(gamePath);
    LOGF_PS5_DIAG("KiSystemStartup: BuildPathCache end entries={}", g_pathCache.size());

    XamRegisterContent(gameContent, gamePath);
    LOGF_PS5_DIAG("KiSystemStartup: registered game content root={}", gamePath);

    const auto saveFilePath = GetSaveFilePath(true);
    bool saveFileExists = std::filesystem::exists(saveFilePath);
    LOGF_PS5_DIAG("KiSystemStartup: save_path={} exists={}", saveFilePath.string(), saveFileExists);

    if (!saveFileExists)
    {
        // Copy base save data to modded save as fallback.
#if defined(__PROSPERO__)
        (void)mkdir(GetUserPath().string().c_str(), 0777);
        (void)mkdir(saveFilePath.parent_path().string().c_str(), 0777);
#endif
        std::error_code ec;
        std::filesystem::create_directories(saveFilePath.parent_path(), ec);

        if (!ec)
        {
            std::filesystem::copy_file(GetSaveFilePath(false), saveFilePath, ec);
            saveFileExists = !ec;
        }
    }

    if (saveFileExists)
    {
        std::u8string savePathU8 = saveFilePath.parent_path().u8string();
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "SonicNextSaveData.bin"), (const char*)(savePathU8.c_str()));
    }

    // Mount game
    LOG_PS5_DIAG("KiSystemStartup: game-content mount begin root=game");
    const uint32_t gameMountResult = XamContentCreateEx(0, "game", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
    LOGF_PS5_DIAG("KiSystemStartup: game-content mount end result=0x{:08x}", gameMountResult);

    // OS mounts game data to D:
    LOG_PS5_DIAG("KiSystemStartup: game-content mount begin root=D");
    const uint32_t driveMountResult = XamContentCreateEx(0, "D", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
    LOGF_PS5_DIAG("KiSystemStartup: game-content mount end root=D result=0x{:08x}", driveMountResult);

    std::error_code ec;
    size_t dlcDirectories = 0;
    LOGF_PS5_DIAG("KiSystemStartup: DLC scan begin path={}", (GetGamePath() / "dlc").string());
    for (auto& file : std::filesystem::directory_iterator(GetGamePath() / "dlc", ec))
    {
        if (file.is_directory())
        {
            std::u8string fileNameU8 = file.path().filename().u8string();
            std::u8string filePathU8 = file.path().u8string();
            XamRegisterContent(XamMakeContent(XCONTENTTYPE_DLC, (const char*)(fileNameU8.c_str())), (const char*)(filePathU8.c_str()));
            ++dlcDirectories;
            LOGF_PS5_DIAG("KiSystemStartup: registered DLC name={} path={}", file.path().filename().string(), file.path().string());
        }
    }
    LOGF_PS5_DIAG("KiSystemStartup: DLC scan end directories={} error_value={} error_message={}", dlcDirectories, ec.value(), ec.message());

    LOG_PS5_DIAG("KiSystemStartup: XAudioInitializeSystem begin");
    XAudioInitializeSystem();
    LOG_PS5_DIAG("KiSystemStartup: XAudioInitializeSystem end");
}

uint32_t LdrLoadModule(const std::filesystem::path &path)
{
    LOGF_PS5_DIAG("LdrLoadModule begin path={}", path.string());
    const auto loadResult = LoadFile(path);
    LOGF_PS5_DIAG("LdrLoadModule file read complete bytes={}", loadResult.size());
    if (loadResult.empty())
    {
        LOGN_ERROR("LdrLoadModule failed: executable file was empty or unreadable.");
        assert("Failed to load module" && false);
        return 0;
    }

    LOG_PS5_DIAG("LdrLoadModule image parse begin");
    const auto image = Image::ParseImage(loadResult.data(), loadResult.size());
    LOGF_PS5_DIAG("LdrLoadModule image parsed base=0x{:x} size=0x{:x} entry=0x{:x} resource_offset=0x{:x} resource_size=0x{:x}",
                  image.base, image.size, image.entry_point, image.resource_offset, image.resource_size);

    void* guestImageDestination = g_memory.Translate(image.base);
    LOGF_PS5_DIAG("LdrLoadModule image copy begin destination=0x{:x} source=0x{:x} size=0x{:x}",
                  reinterpret_cast<uintptr_t>(guestImageDestination), reinterpret_cast<uintptr_t>(image.data.get()), image.size);
    memcpy(guestImageDestination, image.data.get(), image.size);
    LOG_PS5_DIAG("LdrLoadModule image copy end");
    g_xdbfWrapper = XDBFWrapper(static_cast<uint8_t*>(g_memory.Translate(image.resource_offset)), image.resource_size);
    LOG_PS5_DIAG("LdrLoadModule XDBF resources initialized");

    return image.entry_point;
}

#ifdef __x86_64__
__attribute__((constructor(101), target("no-avx,no-avx2"), noinline))
void init()
{
#if defined(__PROSPERO__)
    os::logger::Init();
    LOG_PS5_DIAG("early constructor entered");
#endif
    uint32_t eax, ebx, ecx, edx;

    // Execute CPUID for processor info and feature bits.
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
#if defined(__PROSPERO__)
    LOG_PS5_DIAG("early constructor: CPUID returned");
#endif

    // Check for AVX support.
    if ((ecx & (1 << 28)) == 0)
    {
        printf("[*] CPU does not support the AVX instruction set.\n");

#ifdef _WIN32
        MessageBoxA(nullptr, "Your CPU does not meet the minimum system requirements.", "Marathon Recompiled", MB_ICONERROR);
#endif

        MarathonExit(1);
    }
#if defined(__PROSPERO__)
    LOG_PS5_DIAG("early constructor: AVX check passed; returning");
#endif
}
#endif

int main(int argc, char *argv[])
{
    // Init is idempotent. Calling it here retries the actual log-file open if
    // the early constructor could not obtain a log descriptor.
    os::logger::Init();
    // Keep the very first post-constructor checkpoint free of fmt formatting;
    // startup will log detailed option values after main is confirmed entered.
    LOG_PS5_DIAG("========== main entered ==========");

#ifdef _WIN32
    timeBeginPeriod(1);
#endif
#if defined(__PROSPERO__)
    LOG_PS5_DIAG("phase=hide_splash begin");
    const int hideSplashResult = sceSystemServiceHideSplashScreen();
    LOGF_PS5_DIAG("phase=hide_splash end result={}", hideSplashResult);
#endif
    LOG_PS5_DIAG("phase=initialize_user_path begin");
    InitializeUserPath();
    LOGF_PS5_DIAG("phase=initialize_user_path end user_path={} executable_root={}", GetUserPath().string(), g_executableRoot.string());

    LOG_PS5_DIAG("phase=check_console begin");
    os::process::CheckConsole();
    LOG_PS5_DIAG("phase=check_console end");

    LOG_PS5_DIAG("phase=registry_init begin");
    const bool registryInitialized = os::registry::Init();
    LOGF_PS5_DIAG("phase=registry_init end result={}", registryInitialized);
    if (!registryInitialized)
        LOGN_WARNING("OS does not support registry.");

    LOG_PS5_DIAG("phase=preload_executable begin");
    PreloadContext preloadContext;
    preloadContext.PreloadExecutable();
    LOG_PS5_DIAG("phase=preload_executable end");

    bool forceInstaller = false;
    bool forceDLCInstaller = false;
    bool useDefaultWorkingDirectory = false;
    bool forceInstallationCheck = false;
    bool graphicsApiRetry = false;
    const char *sdlVideoDriver = nullptr;

    for (uint32_t i = 1; i < argc; i++)
    {
        forceInstaller = forceInstaller || (strcmp(argv[i], "--install") == 0);
        forceDLCInstaller = forceDLCInstaller || (strcmp(argv[i], "--install-dlc") == 0);
        useDefaultWorkingDirectory = useDefaultWorkingDirectory || (strcmp(argv[i], "--use-cwd") == 0);
        forceInstallationCheck = forceInstallationCheck || (strcmp(argv[i], "--install-check") == 0);
        graphicsApiRetry = graphicsApiRetry || (strcmp(argv[i], "--graphics-api-retry") == 0);
        App::s_isSkipLogos = App::s_isSkipLogos || (strcmp(argv[i], "--skip-logos") == 0);

        if (strcmp(argv[i], "--sdl-video-driver") == 0)
        {
            if ((i + 1) < argc)
                sdlVideoDriver = argv[++i];
            else
                LOGN_WARNING("No argument was specified for --sdl-video-driver. Option will be ignored.");
        }
    }

    LOGF_PS5_DIAG("options parsed forceInstaller={} forceDLCInstaller={} useDefaultWorkingDirectory={} forceInstallationCheck={} graphicsApiRetry={} skipLogos={} sdlVideoDriver={}",
                  forceInstaller, forceDLCInstaller, useDefaultWorkingDirectory, forceInstallationCheck, graphicsApiRetry,
                  App::s_isSkipLogos, sdlVideoDriver != nullptr ? sdlVideoDriver : "<default>");

    if (!useDefaultWorkingDirectory)
    {
        // Set the current working directory to the executable's path.
        const auto executablePath = os::process::GetExecutablePath();
        LOGF_PS5_DIAG("phase=set_working_directory begin executable={}", executablePath.string());
#if defined(__PROSPERO__)
        // Avoid libc++ std::filesystem::current_path on Prospero; use the
        // platform process wrapper so a filesystem implementation issue cannot
        // turn this optional working-directory change into a null call.
        LOG_PS5_DIAG("phase=set_working_directory chdir begin target=/app0");
        const bool workingDirectorySet = os::process::SetWorkingDirectory(g_executableRoot);
        const int workingDirectoryError = workingDirectorySet ? 0 : errno;
        LOG_PS5_DIAG("phase=set_working_directory chdir returned");
        LOGF_PS5_DIAG("phase=set_working_directory end result={} errno={}",
                      workingDirectorySet, workingDirectoryError);
#else
        std::error_code ec;
        std::filesystem::current_path(executablePath.parent_path(), ec);
        LOGF_PS5_DIAG("phase=set_working_directory end cwd={} error_value={} error_message={}",
                      std::filesystem::current_path().string(), ec.value(), ec.message());
#endif
    }

    LOG_PS5_DIAG("phase=config_load begin");
    Config::Load();
    LOGF_PS5_DIAG("phase=config_load end graphics_api={} graphics_device={} anti_aliasing={} resolution_scale={} vsync={} triple_buffering={}",
                  static_cast<int>(Config::GraphicsAPI.Value), Config::GraphicsDevice.Value,
                  static_cast<int>(Config::AntiAliasing.Value), static_cast<double>(Config::ResolutionScale.Value),
                  Config::VSync.Value, static_cast<int>(Config::TripleBuffering.Value));

    if (forceInstallationCheck)
    {
        // Create the console to show progress to the user, otherwise it will seem as if the game didn't boot at all.
        os::process::ShowConsole();

        Journal journal;
        double lastProgressMiB = 0.0;
        double lastTotalMib = 0.0;
        Installer::checkInstallIntegrity(GetGamePath(), journal, [&]()
        {
            constexpr double MiBDivisor = 1024.0 * 1024.0;
            constexpr double MiBProgressThreshold = 128.0;
            double progressMiB = double(journal.progressCounter) / MiBDivisor;
            double totalMiB = double(journal.progressTotal) / MiBDivisor;
            if (journal.progressCounter > 0)
            {
                if ((progressMiB - lastProgressMiB) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Checking files: %0.2f MiB / %0.2f MiB\n", progressMiB, totalMiB);
                    lastProgressMiB = progressMiB;
                }
            }
            else
            {
                if ((totalMiB - lastTotalMib) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Scanning files: %0.2f MiB\n", totalMiB);
                    lastTotalMib = totalMiB;
                }
            }

            return true;
        });

        char resultText[512];
        uint32_t messageBoxStyle;
        if (journal.lastResult == Journal::Result::Success)
        {
            snprintf(resultText, sizeof(resultText), "%s", Localise("IntegrityCheck_Success").c_str());
            fprintf(stdout, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_INFORMATION;
        }
        else
        {
            snprintf(resultText, sizeof(resultText), Localise("IntegrityCheck_Failed").c_str(), journal.lastErrorMessage.c_str());
            fprintf(stderr, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_ERROR;
        }

        SDL_ShowSimpleMessageBox(messageBoxStyle, GameWindow::GetTitle(), resultText, GameWindow::s_pWindow);
        MarathonExit(int(journal.lastResult));
    }

#if defined(_WIN32) && defined(MARATHON_RECOMP_D3D12)
    for (auto& dll : g_D3D12RequiredModules)
    {
        if (!std::filesystem::exists(g_executableRoot / dll))
        {
            char text[512];
            snprintf(text, sizeof(text), Localise("System_Win32_MissingDLLs").c_str(), dll.data());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), text, GameWindow::s_pWindow);
            MarathonExit(1);
        }
    }
#endif

#if !defined(__PROSPERO__)
    // Check the time since the last time an update was checked. Store the new time if the difference is more than six hours.
    constexpr double TimeBetweenUpdateChecksInSeconds = 6 * 60 * 60;
    time_t timeNow = std::time(nullptr);
    double timeDifferenceSeconds = difftime(timeNow, Config::LastChecked);
    if (timeDifferenceSeconds > TimeBetweenUpdateChecksInSeconds)
    {
        UpdateChecker::initialize();
        UpdateChecker::start();
        Config::LastChecked = timeNow;
        Config::Save();
    }
#endif

    LOGF_PS5_DIAG("phase=host_startup begin showConsole={}", Config::ShowConsole.Value);
    if (Config::ShowConsole)
        os::process::ShowConsole();
    LOGN_WARNING("Host Startup");
    HostStartup();
    LOG_PS5_DIAG("phase=host_startup end");

    std::filesystem::path modulePath;
    LOGF_PS5_DIAG("phase=check_game_install begin game_root={}", GetGamePath().string());
    const bool isGameInstalled = Installer::checkGameInstall(GetGamePath(), modulePath);
    LOGF_PS5_DIAG("phase=check_game_install end installed={} module_path={}", isGameInstalled, modulePath.string());
    bool runInstallerWizard = forceInstaller || forceDLCInstaller || !isGameInstalled;
    LOGF_PS5_DIAG("phase=installer_decision forceInstaller={} forceDLCInstaller={} runInstallerWizard={}",
                  forceInstaller, forceDLCInstaller, runInstallerWizard);
    if (runInstallerWizard)
    {
#if defined(__PROSPERO__)
        LOG_PS5_DIAG("phase=installer_hide_splash begin");
        const int hideSplashResult = sceSystemServiceHideSplashScreen();
        LOGF_PS5_DIAG("phase=installer_hide_splash end result={}", hideSplashResult);
#endif
        LOGF_PS5_DIAG("phase=installer_video_init begin sdlVideoDriver={} retry={}",
                      sdlVideoDriver != nullptr ? sdlVideoDriver : "<default>", graphicsApiRetry);
        const bool installerVideoReady = Video::CreateHostDevice(sdlVideoDriver, graphicsApiRetry);
        LOGF_PS5_DIAG("phase=installer_video_init end result={}", installerVideoReady);
        if (!installerVideoReady)
        {
            LOGN_ERROR("Installer video device initialization failed.");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
            MarathonExit(1);
        }

        LOGF_PS5_DIAG("phase=installer_wizard begin forceDLC={}", isGameInstalled && forceDLCInstaller);
        const bool installerCompleted = InstallerWizard::Run(GetGamePath(), isGameInstalled && forceDLCInstaller);
        LOGF_PS5_DIAG("phase=installer_wizard end result={}", installerCompleted);
        if (!installerCompleted)
            MarathonExit(0);
    }

    ModLoader::Init();

    LOG_PS5_DIAG("phase=KiSystemStartup begin");
    KiSystemStartup();
    LOG_PS5_DIAG("phase=KiSystemStartup end");

    LOGF_PS5_DIAG("phase=LdrLoadModule begin module_path={}", modulePath.string());
    uint32_t entry = LdrLoadModule(modulePath);
    LOGF_PS5_DIAG("phase=LdrLoadModule end entry=0x{:08x}", entry);

    if (!runInstallerWizard)
    {
#if defined(__PROSPERO__)
        LOG_PS5_DIAG("phase=game_hide_splash begin");
        const int hideSplashResult = sceSystemServiceHideSplashScreen();
        LOGF_PS5_DIAG("phase=game_hide_splash end result={}", hideSplashResult);
#endif
        LOGF_PS5_DIAG("phase=game_video_init begin sdlVideoDriver={} retry={}",
                      sdlVideoDriver != nullptr ? sdlVideoDriver : "<default>", graphicsApiRetry);
        const bool gameVideoReady = Video::CreateHostDevice(sdlVideoDriver, graphicsApiRetry);
        LOGF_PS5_DIAG("phase=game_video_init end result={}", gameVideoReady);
        if (!gameVideoReady)
        {
            LOGN_ERROR("Game video device initialization failed.");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
            MarathonExit(1);
        }
    }
    LOGN_WARNING("Start Guest Thread");
    LOGN_WARNING(modulePath.string());
    // Video::StartPipelinePrecompilation();

    LOGF_PS5_DIAG("phase=GuestThread::Start begin entry=0x{:08x}", entry);
    const uint32_t guestResult = GuestThread::Start({ entry, 0, 0 });
    LOGF_PS5_DIAG("phase=GuestThread::Start returned result=0x{:08x}", guestResult);

    return 0;
}

GUEST_FUNCTION_STUB(__imp__vsprintf);
GUEST_FUNCTION_STUB(__imp___vsnprintf);
GUEST_FUNCTION_STUB(__imp__sprintf);
GUEST_FUNCTION_STUB(__imp___snprintf);
GUEST_FUNCTION_STUB(__imp___snwprintf);
GUEST_FUNCTION_STUB(__imp__vswprintf);
GUEST_FUNCTION_STUB(__imp___vscwprintf);
GUEST_FUNCTION_STUB(__imp__swprintf);
