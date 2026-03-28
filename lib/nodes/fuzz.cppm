module;
#include "miniaudio.h"
#include <atomic>

export module fuzz;

import node_base;
import processor_graph;
import effect_node;

export class Fuzz : public EffectNode
{
public:
    Fuzz(ProcessorGraph& graph, ma_uint32 channels, float gain = 10.0f, float threshold = 0.3f);

    void setGain(float gain) { gain_.store(gain, std::memory_order_relaxed); }
    void setThreshold(float threshold) { threshold_.store(threshold, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 2; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"gain", 1.0f, 100.0f, 10.0f, "x"};
            case 1: return {"threshold", 0.0f, 1.0f, 0.3f, ""};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return gain_.load(std::memory_order_relaxed);
            case 1: return threshold_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: gain_.store(value, std::memory_order_relaxed); break;
            case 1: threshold_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    std::atomic<float> gain_;
    std::atomic<float> threshold_;
    ma_uint32 channels_;
};

Fuzz::Fuzz(ProcessorGraph& graph, ma_uint32 channels, float gain, float threshold)
    : EffectNode(graph, channels), gain_(gain), threshold_(threshold), channels_(channels)
{
}

void Fuzz::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    float gain = gain_.load(std::memory_order_relaxed);
    float threshold = threshold_.load(std::memory_order_relaxed);

    for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
    {
        float sample = pInput[i] * gain;

        if (sample > threshold)
            sample = threshold;
        else if (sample < -threshold)
            sample = -threshold;

        pOutput[i] = sample;
    }
}
