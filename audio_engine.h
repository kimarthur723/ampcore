#pragma once
#include "miniaudio.h"
#include "processor_graph.h"
#include <memory>

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

    ProcessorGraph& getGraph();
    ma_uint32 getSampleRate() const;
    ma_uint32 getChannels() const;
    void start();
    void stop();
    bool isStarted() const;


private:
    ma_device device_;
    std::unique_ptr<ProcessorGraph> graph_;

    static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void processAudio(float* pOutput, const float* pInput, ma_uint32 frameCount);
};
