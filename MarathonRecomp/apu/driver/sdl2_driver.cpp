#include <apu/audio.h>
#include <cpu/guest_thread.h>
#include <kernel/heap.h>
#include <os/logger.h>
#include <user/config.h>
#include <ui/game_window.h>

#if defined(__PROSPERO__)
#include <algorithm>
#include <cstdint>
extern "C"
{
    int32_t sceAudioOutInit(void);
    int32_t sceAudioOutOpen(int32_t userId, int32_t type, int32_t index,
                            uint32_t len, uint32_t freq, uint32_t param);
    int32_t sceAudioOutOutput(int32_t handle, const void* ptr);
    int32_t sceAudioOutClose(int32_t handle);
}
static int32_t s_ps5AudioPort = -1;
static constexpr uint32_t PS5_AUDIO_OUT_ALREADY_INITIALIZED = 0x8026000e;
#endif

static PPCFunc* g_clientCallback{};
static uint32_t g_clientCallbackParam{}; // pointer in guest memory
static SDL_AudioDeviceID g_audioDevice{};
static bool g_downMixToStereo;

static void CreateAudioDevice()
{
    LOG_PS5_DIAG("audio: CreateAudioDevice begin");
#if defined(__PROSPERO__)
    if (s_ps5AudioPort > 0)
    {
        LOGF_PS5_DIAG("audio: closing previous sceAudioOut port={}", s_ps5AudioPort);
        const int32_t closeResult = sceAudioOutClose(s_ps5AudioPort);
        LOGF_PS5_DIAG("audio: sceAudioOutClose result={}", closeResult);
        s_ps5AudioPort = -1;
    }

    LOG_PS5_DIAG("audio: sceAudioOutInit begin");
    const int32_t initResult = sceAudioOutInit();
    LOGF_PS5_DIAG("audio: sceAudioOutInit returned {} (0x{:08x})", initResult, static_cast<uint32_t>(initResult));
    if (initResult == 0 || static_cast<uint32_t>(initResult) == PS5_AUDIO_OUT_ALREADY_INITIALIZED)
    {
        // Grain is 256 frames at 48 kHz; the game submits 6-channel float frames.
        LOGF_PS5_DIAG("audio: sceAudioOutOpen begin user=0xff type=0 index=0 len={} freq={} param=1", XAUDIO_NUM_SAMPLES, XAUDIO_SAMPLES_HZ);
        s_ps5AudioPort = sceAudioOutOpen(0xff, 0, 0, XAUDIO_NUM_SAMPLES, XAUDIO_SAMPLES_HZ, 1);
        LOGF_PS5_DIAG("audio: sceAudioOutOpen returned port={} (0x{:08x})", s_ps5AudioPort, static_cast<uint32_t>(s_ps5AudioPort));
        if (s_ps5AudioPort > 0)
        {
            g_downMixToStereo = true;
            LOGFN("Opened PS5 sceAudioOut port={} (48000 Hz, stereo output)", s_ps5AudioPort);
            LOG_PS5_DIAG("audio: direct PS5 output selected");
            return;
        }
        LOGFN_WARNING("sceAudioOutOpen failed ({}); falling back to SDL audio", s_ps5AudioPort);
    }
    else
    {
        LOGFN_WARNING("sceAudioOutInit failed ({}); falling back to SDL audio", initResult);
    }
#endif

    if (g_audioDevice != 0)
    {
        SDL_CloseAudioDevice(g_audioDevice);
        g_audioDevice = 0;
    }

    const bool surround = Config::ChannelConfiguration == EChannelConfiguration::Surround;
    const int allowedChanges = surround ? SDL_AUDIO_ALLOW_CHANNELS_CHANGE : 0;

    SDL_AudioSpec desired{}, obtained{};
    desired.freq = XAUDIO_SAMPLES_HZ;
    desired.format = AUDIO_F32SYS;
    desired.channels = surround ? XAUDIO_NUM_CHANNELS : 2;
    desired.samples = XAUDIO_NUM_SAMPLES;
    g_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, allowedChanges);

    if (obtained.channels != 2 && obtained.channels != XAUDIO_NUM_CHANNELS)
    {
        SDL_CloseAudioDevice(g_audioDevice);
        g_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    }

    if (!g_audioDevice)
        LOGFN_ERROR("Failed to open audio device: {}", SDL_GetError());

    g_downMixToStereo = (obtained.channels == 2);
    LOGF_PS5_DIAG("audio: SDL_OpenAudioDevice result={} obtained_freq={} format=0x{:x} channels={} samples={} downmix={}",
                  g_audioDevice, obtained.freq, obtained.format, obtained.channels, obtained.samples, g_downMixToStereo);
}

void XAudioInitializeSystem()
{
    LOG_PS5_DIAG("audio: XAudioInitializeSystem begin");
#if defined(__PROSPERO__)
    CreateAudioDevice();
    if (s_ps5AudioPort > 0)
    {
        LOGF_PS5_DIAG("audio: XAudioInitializeSystem using direct port={}", s_ps5AudioPort);
        return;
    }
#endif

#ifdef _WIN32
    // Force wasapi on Windows.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    LOG_PS5_DIAG("audio: initializing SDL audio fallback");
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME, "Marathon Recompiled");

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        LOGFN_ERROR("Failed to init audio subsystem: {}", SDL_GetError());
        LOGF_PS5_DIAG("audio: SDL_InitSubSystem failed error={}", SDL_GetError());
        return;
    }

    CreateAudioDevice();
    LOG_PS5_DIAG("audio: XAudioInitializeSystem end");
}

static std::unique_ptr<std::thread> g_audioThread;
static volatile bool g_audioThreadShouldExit;

static void AudioThread()
{
    using namespace std::chrono_literals;
    LOG_PS5_DIAG("audio: AudioThread starting; creating guest thread context");
    GuestThreadContext ctx(0);

    const size_t channels = g_downMixToStereo ? 2 : XAUDIO_NUM_CHANNELS;
    uint32_t traceIterations = 0;
    LOGF_PS5_DIAG("audio: AudioThread ready channels={} downmix={} port={} device={}",
                  channels, g_downMixToStereo,
#if defined(__PROSPERO__)
                  s_ps5AudioPort,
#else
                  -1,
#endif
                  g_audioDevice);
    while (!g_audioThreadShouldExit)
    {
        const bool directPs5Output =
#if defined(__PROSPERO__)
            s_ps5AudioPort > 0;
#else
            false;
#endif
        const uint32_t queuedAudioSize = directPs5Output ? 0 : SDL_GetQueuedAudioSize(g_audioDevice);
        constexpr size_t MAX_LATENCY = 10;
        const size_t callbackAudioSize = channels * XAUDIO_NUM_SAMPLES * sizeof(float);

        if (traceIterations < 4)
            LOGF_PS5_DIAG("audio: callback iteration={} direct={} queued_bytes={} callback_bytes={} guest_callback=0x{:x} guest_param=0x{:08x}",
                          traceIterations, directPs5Output, queuedAudioSize, callbackAudioSize,
                          reinterpret_cast<uintptr_t>(g_clientCallback), g_clientCallbackParam);

        if (directPs5Output || (queuedAudioSize / callbackAudioSize) <= MAX_LATENCY)
        {
            ctx.ppcContext.r3.u32 = g_clientCallbackParam;
            g_clientCallback(ctx.ppcContext, g_memory.base);
            if (traceIterations < 4)
                LOGF_PS5_DIAG("audio: guest callback iteration={} returned r3=0x{:08x}", traceIterations, ctx.ppcContext.r3.u32);
        }
        else if (traceIterations < 4)
        {
            LOGF_PS5_DIAG("audio: guest callback skipped iteration={} latency_frames={}",
                          traceIterations, queuedAudioSize / callbackAudioSize);
        }
        ++traceIterations;

        const auto now = std::chrono::steady_clock::now();
        constexpr auto INTERVAL = 1000000000ns * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ;
        const auto next = now + (INTERVAL - now.time_since_epoch() % INTERVAL);
        std::this_thread::sleep_for(std::chrono::floor<std::chrono::milliseconds>(next - now));
        while (std::chrono::steady_clock::now() < next)
            std::this_thread::yield();
    }
}

static void CreateAudioThread()
{
    if (g_audioDevice != 0)
        SDL_PauseAudioDevice(g_audioDevice, 0);
    g_audioThreadShouldExit = false;
    g_audioThread = std::make_unique<std::thread>(AudioThread);
}

void XAudioRegisterClient(PPCFunc* callback, uint32_t param)
{
    LOGF_PS5_DIAG("audio: XAudioRegisterClient callback=0x{:x} guest_param=0x{:08x}",
                  reinterpret_cast<uintptr_t>(callback), param);
    auto* pClientParam = static_cast<uint32_t*>(g_userHeap.Alloc(sizeof(param)));
    LOGF_PS5_DIAG("audio: client parameter allocation host=0x{:x}", reinterpret_cast<uintptr_t>(pClientParam));
    ByteSwapInplace(param);
    *pClientParam = param;
    g_clientCallbackParam = g_memory.MapVirtual(pClientParam);
    g_clientCallback = callback;
    LOGF_PS5_DIAG("audio: registered callback guest_param=0x{:08x} mapped_param_host=0x{:x}",
                  g_clientCallbackParam, reinterpret_cast<uintptr_t>(pClientParam));

    LOG_PS5_DIAG("audio: CreateAudioThread begin");
    CreateAudioThread();
    LOG_PS5_DIAG("audio: CreateAudioThread returned");
}

static void SubmitHostAudio(const float* samples, size_t sampleCount)
{
#if defined(__PROSPERO__)
    if (s_ps5AudioPort > 0)
    {
        alignas(16) std::array<int16_t, 2 * XAUDIO_NUM_SAMPLES> pcm16{};
        const size_t count = std::min(sampleCount, pcm16.size());
        for (size_t i = 0; i < count; ++i)
        {
            const float sample = std::clamp(samples[i], -1.0f, 1.0f);
            pcm16[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        const int32_t result = sceAudioOutOutput(s_ps5AudioPort, pcm16.data());
        if (result < 0)
            LOGFN_ERROR("sceAudioOutOutput failed: {}", result);
        return;
    }
#endif
    if (g_audioDevice != 0)
        SDL_QueueAudio(g_audioDevice, samples, sampleCount * sizeof(float));
}

void XAudioSubmitFrame(void* samples)
{
    auto floatSamples = reinterpret_cast<be<float>*>(samples);
    float volume = Config::MasterVolume.Value;
    if (Config::MuteOnFocusLost && !GameWindow::s_isFocused)
        volume = 0.0f;

    if (g_downMixToStereo)
    {
        std::array<float, 2 * XAUDIO_NUM_SAMPLES> audioFrames{};
        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
        {
            // 5.1 channel order: front L/R, center, LFE, rear L/R.
            const float ch0 = floatSamples[0 * XAUDIO_NUM_SAMPLES + i];
            const float ch1 = floatSamples[1 * XAUDIO_NUM_SAMPLES + i];
            const float ch2 = floatSamples[2 * XAUDIO_NUM_SAMPLES + i];
            const float ch4 = floatSamples[4 * XAUDIO_NUM_SAMPLES + i];
            const float ch5 = floatSamples[5 * XAUDIO_NUM_SAMPLES + i];
            const float left = (ch0 + ch2 * 0.75f + ch4) * volume;
            const float right = (ch1 + ch2 * 0.75f + ch5) * volume;
            audioFrames[i * 2] = std::isnan(left) ? 0.0f : left;
            audioFrames[i * 2 + 1] = std::isnan(right) ? 0.0f : right;
        }
        SubmitHostAudio(audioFrames.data(), audioFrames.size());
    }
    else
    {
        std::array<float, XAUDIO_NUM_CHANNELS * XAUDIO_NUM_SAMPLES> audioFrames{};
        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
        {
            for (size_t j = 0; j < XAUDIO_NUM_CHANNELS; ++j)
            {
                const float sample = floatSamples[j * XAUDIO_NUM_SAMPLES + i] * volume;
                audioFrames[i * XAUDIO_NUM_CHANNELS + j] = std::isnan(sample) ? 0.0f : sample;
            }
        }
        SubmitHostAudio(audioFrames.data(), audioFrames.size());
    }
}

void XAudioConfigValueChangedCallback(IConfigDef* configDef)
{
    if (configDef == &Config::ChannelConfiguration)
    {
        if (g_audioThread && g_audioThread->joinable())
        {
            g_audioThreadShouldExit = true;
            g_audioThread->join();
        }

        CreateAudioDevice();
        CreateAudioThread();
    }
}
