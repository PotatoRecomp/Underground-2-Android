// mixer output through an SDL audio stream (AAudio underneath): the 5.1 mix folded to the stereo pair Android plays
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

#include "audio/output.h"
#include "common/music.h"
#include "core/log.h"

namespace audio {

namespace {

constexpr float kCentreFold = 0.707f;
constexpr uint32_t kBlock = kRate / 100;  // frames the mixer renders a call: 10 ms

std::atomic<uint32_t> g_mode{kOutputAuto};
std::atomic<bool> g_paused{false};
SDL_AudioStream* g_stream = nullptr;

struct Pump {
    std::function<void(float*, uint32_t)> render;
    std::vector<float> mix = std::vector<float>(kBlock * kChannels);
    std::vector<float> out;  // stereo frames rendered, not yet handed to SDL
};

// past the knee a sample bends towards full scale rather than being cut at it (the stream clips at 1): the fold sums the
// speakers, and My Music over a race's engines went past full scale and cracked. Under the knee nothing changes
constexpr float kKnee = 0.9f, kOver = 1.0f - kKnee;
float Limit(float x) {
    float a = std::fabs(x);
    if (a <= kKnee) return x;
    return std::copysign(kKnee + kOver * std::tanh((a - kKnee) / kOver), x);
}

// L = FL + kCentreFold C + BL and R = FR + kCentreFold C + BR without the sub; mono hears the average of the two
void Fold(const float* in, uint32_t frames, bool mono, std::vector<float>& out) {
    for (uint32_t i = 0; i < frames; i++) {
        const float* f = in + size_t(i) * kChannels;
        float c = f[Centre] * kCentreFold;
        float l = f[FrontLeft] + c + f[BackLeft];
        float r = f[FrontRight] + c + f[BackRight];
        if (mono) l = r = (l + r) * 0.5f;
        out.push_back(Limit(l));
        out.push_back(Limit(r));
    }
}

void SDLCALL Feed(void* user, SDL_AudioStream* stream, int additional, int) {
    auto& p = *static_cast<Pump*>(user);
    constexpr int kFrameBytes = 2 * sizeof(float);
    size_t want = size_t(std::max(additional, 0) / kFrameBytes) * 2;
    while (p.out.size() < want) {
        p.render(p.mix.data(), kBlock);
        music::Mix(p.mix.data(), kBlock);
        Fold(p.mix.data(), kBlock, g_mode == kOutputMono, p.out);
    }
    if (!want) return;
    SDL_PutAudioStreamData(stream, p.out.data(), int(want * sizeof(float)));
    p.out.erase(p.out.begin(), p.out.begin() + std::ptrdiff_t(want));
}

}  // namespace

// a 5.1 mix only when the player asks for it: phones and tablets play stereo
bool IsSurround() { return g_mode == kOutputSurround; }

bool IsMono() { return g_mode == kOutputMono; }

void SetOutputMode(uint32_t mode) {
    if (g_mode.exchange(mode) != mode) LOGI("audio: output %u", mode);
}

void SetPaused(bool paused) {
    if (g_paused.exchange(paused) == paused || !g_stream) return;
    if (paused) SDL_PauseAudioStreamDevice(g_stream);
    else SDL_ResumeAudioStreamDevice(g_stream);
}

void StartOutput(std::function<void(float*, uint32_t)> render) {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        LOGE("audio: SDL audio unavailable, no sound: %s", SDL_GetError());
        return;
    }
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(kRate)};
    auto* pump = new Pump{std::move(render)};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, pump);
    if (!g_stream) {
        LOGE("audio: no output device, no sound: %s", SDL_GetError());
        delete pump;
        return;
    }
    LOGI("audio: %s output through %s", IsSurround() ? "5.1 folded to stereo" : "stereo", SDL_GetCurrentAudioDriver());
    if (!g_paused) SDL_ResumeAudioStreamDevice(g_stream);
}

}  // namespace audio
