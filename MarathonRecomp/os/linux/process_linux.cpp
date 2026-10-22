#include <os/process.h>

#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

std::filesystem::path os::process::GetExecutablePath()
{
#if defined(__PROSPERO__)
    return std::filesystem::path("/app0/eboot.bin");
#else
    char exePath[PATH_MAX] = {};
    if (readlink("/proc/self/exe", exePath, PATH_MAX) > 0)
    {
        return std::filesystem::path(std::u8string_view((const char8_t*)(exePath)));
    }
    else
    {
        return std::filesystem::path();
    }
#endif
}

std::filesystem::path os::process::GetExecutableRoot()
{
#if defined(__PROSPERO__)
    return std::filesystem::path("/app0");
#else
    return GetExecutablePath().remove_filename();
#endif
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    char cwd[PATH_MAX] = {};
    char *res = getcwd(cwd, sizeof(cwd));
    if (res != nullptr)
    {
        return std::filesystem::path(std::u8string_view((const char8_t*)(cwd)));
    }
    else
    {
        return std::filesystem::path();
    }
}

bool os::process::SetWorkingDirectory(const std::filesystem::path& path)
{
    return chdir(path.c_str()) == 0;
}

bool os::process::StartProcess(const std::filesystem::path& path, const std::vector<std::string>& args, std::filesystem::path work)
{
#if defined(__PROSPERO__)
    // The PS5 title cannot spawn desktop child processes.
    (void)path;
    (void)args;
    (void)work;
    return false;
#else
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        setsid();
        
        std::u8string workU8 = work.u8string();
        chdir((const char*)(workU8.c_str())); 
        
        std::u8string pathU8 = path.u8string();
        std::vector<char*> argStrs;
        argStrs.push_back((char*)(pathU8.c_str()));
        for (const std::string& arg : args)
            argStrs.push_back((char *)(arg.c_str()));
        
        argStrs.push_back(nullptr);
        execvp((const char*)(pathU8.c_str()), argStrs.data());
        raise(SIGKILL);
    }

    return true;
#endif
}

void os::process::CheckConsole()
{
    // The Linux and PS5 builds do not have a separate console window.
    g_consoleVisible = true;
}

void os::process::ShowConsole()
{
    // No separate console window to show on Linux or PS5.
}
