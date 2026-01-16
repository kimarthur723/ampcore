#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_engine.h"
#include <stdexcept>
#include <string>

AudioEngine::AudioEngine(
    ma_uint32 sampleRate,
    ma_uint32 captureChannels,
    ma_uint32 playbackChannels,
    ma_uint32 framesPerBuffer)
{
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);

    // capture settings
    config.capture.format = ma_format_f32;
    config.capture.channels = captureChannels;

    // playback settings
    config.playback.format = ma_format_f32;
    config.playback.channels = playbackChannels;

    // other settings
    config.sampleRate = sampleRate;
    config.periodSizeInFrames = framesPerBuffer;
    config.pUserData = this;
    config.dataCallback = audioCallback;

    ma_result result = ma_device_init(nullptr, &config, &device_);

    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(
            std::string("Failed to initialize audio device: ") +
            ma_result_description(result)
        );
    }
}

AudioEngine::~AudioEngine()
{
    ma_device_uninit(&device_);
}

void AudioEngine::start()
{
    ma_result result = ma_device_start(&device_);
    
    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(
            std::string("Failed to start audio device: ") +
            ma_result_description(result)
        );
    }
}


void AudioEngine::stop()
{
    ma_device_stop(&device_);
}

bool AudioEngine::isStarted() const
{
    return ma_device_is_started(&device_) == MA_TRUE;
}

void AudioEngine::audioCallback(
    ma_device* pDevice,
    void* pOutput,
    const void* pInput,
    ma_uint32 frameCount)
{
    AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    
    float* output = static_cast<float*>(pOutput);
    const float* input = static_cast<const float*>(pInput);

    engine->processAudio(output, input, frameCount);
}

void AudioEngine::processAudio(
    float* pOutput,
    const float* pInput,
    ma_uint32 frameCount)
{
    ma_uint32 captureChannels = device_.capture.channels;
    ma_uint32 playbackChannels = device_.playback.channels;

    if (captureChannels == playbackChannels)
    {
        for (ma_uint32 i = 0; i < frameCount * captureChannels; ++i)
        {
            pOutput[i] = pInput[i];
        }
    }
    else if (captureChannels == 1 && playbackChannels == 2)
    {
        for (ma_uint32 frame = 0; frame < frameCount; ++frame)
        {
            float sample = pInput[frame];
            pOutput[frame * 2 + 0] = sample;
            pOutput[frame * 2 + 1] = sample;
        }
    }
    else {
        return;
    }
}
