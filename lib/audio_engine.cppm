module;
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdexcept>
#include <string>

export module audio_engine;

import processor_graph;

export class AudioEngine
{
public:
    AudioEngine(ProcessorGraph& graph,
                ma_uint32 sampleRate = 44100,
                ma_uint32 captureChannels = 0,
                ma_uint32 framesPerBuffer = 512);

    ~AudioEngine();

    // non copyable, non movable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    ma_uint32 getSampleRate() const;
    void start();
    void stop();
    bool isStarted() const;

private:
    ProcessorGraph& graph_;
    ma_device device_;

    static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void processAudio(float* pOutput, const float* pInput, ma_uint32 frameCount);
};

AudioEngine::AudioEngine(
    ProcessorGraph& graph,
    ma_uint32 sampleRate,
    ma_uint32 captureChannels,
    ma_uint32 framesPerBuffer)
    : graph_(graph)
{
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);

    // capture settings
    config.capture.format = ma_format_f32;
    config.capture.channels = captureChannels;

    // playback settings
    config.playback.format = ma_format_f32;
    config.playback.channels = graph.getChannels();

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

ma_uint32 AudioEngine::getSampleRate() const
{
    return device_.sampleRate;
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
    (void)pInput;
    graph_.read(pOutput, frameCount);
}
