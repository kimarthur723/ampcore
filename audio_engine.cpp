#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_engine.h"
#include "processor_graph.h"
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

    graph_ = std::make_unique<ProcessorGraph>(
        device_.playback.channels
    );
}

AudioEngine::~AudioEngine()
{
    ma_device_uninit(&device_);
}

ProcessorGraph& AudioEngine::getGraph()
{
    return *graph_;
}

ma_uint32 AudioEngine::getSampleRate() const
{
    return device_.sampleRate;
}

ma_uint32 AudioEngine::getChannels() const
{
    return device_.playback.channels;
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
    graph_->read(pOutput, frameCount);
}
