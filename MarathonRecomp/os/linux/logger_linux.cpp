#include <os/logger.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__PROSPERO__)
namespace
{
    // /app0 is attempted first as requested. Retail-style app content mounts
    // are normally read-only, so the title-data directory is the fallback.
    constexpr const char* APP0_LOG_PATH = "/app0/MarathonRecomp.log";
    constexpr const char* DATA_LOG_DIR = "/data/MarathonRecomp";
    constexpr const char* DATA_LOG_PATH = "/data/MarathonRecomp/MarathonRecomp.log";

    volatile sig_atomic_t g_logFd = -1;
    bool g_loggerInitialized = false;
    char g_activeLogPath[160]{};

    void WriteRaw(const char* data, size_t size)
    {
        const int fd = static_cast<int>(g_logFd);
        if (fd < 0 || data == nullptr || size == 0)
            return;

        // A process crash must not strand records in a userspace stdio buffer.
        // Each log record goes straight to the kernel with write(2).
        while (size != 0)
        {
            const ssize_t written = ::write(fd, data, size);
            if (written <= 0)
                return;
            data += written;
            size -= static_cast<size_t>(written);
        }
    }

    void CopyPath(const char* path)
    {
        size_t i = 0;
        for (; path[i] != '\0' && i + 1 < sizeof(g_activeLogPath); ++i)
            g_activeLogPath[i] = path[i];
        g_activeLogPath[i] = '\0';
    }

    const char* LogTypeName(os::logger::ELogType type)
    {
        switch (type)
        {
        case os::logger::ELogType::Info: return "INFO";
        case os::logger::ELogType::Utility: return "TRACE";
        case os::logger::ELogType::Warning: return "WARN";
        case os::logger::ELogType::Error: return "ERROR";
        default: return "UNKNOWN";
        }
    }

    uintptr_t CurrentThreadToken()
    {
        const pthread_t thread = pthread_self();
        uintptr_t token = 0;
        static_assert(sizeof(thread) <= sizeof(token), "pthread_t is larger than uintptr_t");
        std::memcpy(&token, &thread, sizeof(thread));
        return token;
    }

    void AppendText(char* buffer, size_t capacity, size_t& used, const char* text)
    {
        while (*text != '\0' && used < capacity)
            buffer[used++] = *text++;
    }

    void AppendUnsigned(char* buffer, size_t capacity, size_t& used, uintptr_t value, unsigned base)
    {
        char digits[2 * sizeof(uintptr_t) + 1];
        size_t count = 0;
        do
        {
            const unsigned digit = static_cast<unsigned>(value % base);
            digits[count++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
            value /= base;
        } while (value != 0 && count < sizeof(digits));

        while (count != 0 && used < capacity)
            buffer[used++] = digits[--count];
    }

    void FatalSignalHandler(int signalNumber, siginfo_t* info, void*)
    {
        // Keep this handler async-signal-safe: no fmt, allocation, locks, or
        // stdio. SA_RESETHAND lets the original fault recur with its default
        // disposition after this one record is written, preserving the system
        // crash report and coredump behavior.
        char line[192];
        size_t used = 0;
        AppendText(line, sizeof(line), used, "\n[FATAL_SIGNAL] signal=");
        AppendUnsigned(line, sizeof(line), used, static_cast<uintptr_t>(signalNumber), 10);
        AppendText(line, sizeof(line), used, " si_code=");
        AppendUnsigned(line, sizeof(line), used,
                       static_cast<uintptr_t>(info != nullptr ? info->si_code : 0), 10);
        AppendText(line, sizeof(line), used, " si_addr=0x");
        AppendUnsigned(line, sizeof(line), used,
                       reinterpret_cast<uintptr_t>(info != nullptr ? info->si_addr : nullptr), 16);
        if (used < sizeof(line))
            line[used++] = '\n';
        WriteRaw(line, used);
    }

    void InstallFatalSignalHandlers()
    {
        constexpr int fatalSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
        struct sigaction action{};
        action.sa_sigaction = FatalSignalHandler;
        action.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigemptyset(&action.sa_mask);

        for (const int signalNumber : fatalSignals)
        {
            if (::sigaction(signalNumber, &action, nullptr) != 0)
            {
                char line[96];
                const int length = std::snprintf(line, sizeof(line),
                    "[LOGGER] sigaction failed signal=%d errno=%d\n", signalNumber, errno);
                if (length > 0)
                    WriteRaw(line, static_cast<size_t>(length));
            }
        }
    }
}
#endif

void os::logger::Init()
{
#if defined(__PROSPERO__)
    if (g_logFd >= 0)
        return;
    g_loggerInitialized = true;

    int app0Error = 0;
    int fd = ::open(APP0_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
    if (fd < 0)
    {
        app0Error = errno;
        (void)::mkdir("/data", 0777);
        (void)::mkdir(DATA_LOG_DIR, 0777);
        fd = ::open(DATA_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
        if (fd >= 0)
            CopyPath(DATA_LOG_PATH);
    }
    else
    {
        CopyPath(APP0_LOG_PATH);
    }

    g_logFd = fd;
    if (fd >= 0)
    {
        WriteRaw("========== MarathonRecomp PS5 diagnostic log ==========\n",
                 sizeof("========== MarathonRecomp PS5 diagnostic log ==========\n") - 1);
        char header[512];
        const int length = std::snprintf(header, sizeof(header),
            "preferred_path=%s\nactive_path=%s\napp0_open_errno=%d\n"
            "build_compiled=%s %s\npid=%d\n"
            "records=monotonic_time,pid,thread,severity,function,message\n",
            APP0_LOG_PATH, g_activeLogPath, app0Error, __DATE__, __TIME__, static_cast<int>(::getpid()));
        if (length > 0)
            WriteRaw(header, static_cast<size_t>(length));
    }
    else
    {
        char line[256];
        const int length = std::snprintf(line, sizeof(line),
            "[logger] Could not open %s or %s; errno=%d.\n", APP0_LOG_PATH, DATA_LOG_PATH, errno);
        if (length > 0)
        {
            const size_t bytes = static_cast<size_t>(length) < sizeof(line)
                ? static_cast<size_t>(length) : sizeof(line) - 1;
            (void)::write(STDERR_FILENO, line, bytes);
        }
    }

    static bool handlersInstalled = false;
    if (!handlersInstalled)
    {
        InstallFatalSignalHandlers();
        handlersInstalled = true;
    }
#else
    // Keep the existing desktop Linux logger console-only.
#endif
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
#if defined(__PROSPERO__)
    if (!g_loggerInitialized)
        Init();

    timespec now{};
    (void)::clock_gettime(CLOCK_MONOTONIC, &now);
    const std::string_view message = str;
    const size_t messageSize = message.size() < 3500 ? message.size() : 3500;
    const int messageLength = static_cast<int>(messageSize);

    // Use a bounded stack buffer and write(2) only. This logger is called from
    // a priority-101 constructor, before main and before any console setup.
    char line[4096];
    const int length = std::snprintf(line, sizeof(line),
        "[+%lld.%03d pid=%d tid=0x%llx %s] [%s] %.*s\n",
        static_cast<long long>(now.tv_sec), static_cast<int>(now.tv_nsec / 1000000),
        static_cast<int>(::getpid()), static_cast<unsigned long long>(CurrentThreadToken()),
        LogTypeName(type), func != nullptr ? func : "*", messageLength,
        message.data() != nullptr ? message.data() : "");
    if (length > 0)
    {
        size_t bytes = static_cast<size_t>(length) < sizeof(line)
            ? static_cast<size_t>(length) : sizeof(line) - 1;
        if (static_cast<size_t>(length) >= sizeof(line))
            line[bytes - 1] = '\n';
        WriteRaw(line, bytes);
    }
#else
    (void)type;
    if (func)
        fmt::println("[{}] {}", func, str);
    else
        fmt::println("{}", str);
#endif
}
