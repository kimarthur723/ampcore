#pragma once
#include "miniaudio.h"

// gets audio to and from hardware
class AudioEngine
{
public:
    AudioEngine(ma_uint32 sampleRate = 44100,
                ma_uint32 captureChannels = 0,
                ma_uint32 playbackChannels = 0,
                ma_uint32 framesPerBuffer = 512);

    ~AudioEngine();

    // non copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void start();
    void stop();
    bool isStarted() const;


private:
    ma_device device_;

    static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void processAudio(float* pOutput, const float* pInput, ma_uint32 frameCount);
};
