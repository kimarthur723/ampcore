module;
#include "miniaudio.h"
#include <atomic>
#include <vector>

export module delay;

import node_base;
import processor_graph;
import effect_node;

export class Delay : public EffectNode
{
public:
    Delay(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
          float delaySeconds = 0.5f, float feedback = 0.4f, float mix = 0.5f);

    void setFeedback(float feedback) { feedback_.store(feedback, std::memory_order_relaxed); }
    void setMix(float mix) { mix_.store(mix, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 2; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"feedback", 0.0f, 1.0f, 0.4f, ""};
            case 1: return {"mix", 0.0f, 1.0f, 0.5f, ""};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return feedback_.load(std::memory_order_relaxed);
            case 1: return mix_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: feedback_.store(value, std::memory_order_relaxed); break;
            case 1: mix_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    std::vector<float> buffer_;
    ma_uint32 writePos_;
    ma_uint32 delaySamples_;
    ma_uint32 channels_;
    std::atomic<float> feedback_;
    std::atomic<float> mix_;
};

Delay::Delay(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
             float delaySeconds, float feedback, float mix)
    : EffectNode(graph, channels)
    , writePos_(0)
    , delaySamples_(static_cast<ma_uint32>(delaySeconds * sampleRate) * channels)
    , channels_(channels)
    , feedback_(feedback)
    , mix_(mix)
{
    buffer_.resize(delaySamples_, 0.0f);
}

void Delay::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    ma_uint32 bufferSize = static_cast<ma_uint32>(buffer_.size());
    float feedback = feedback_.load(std::memory_order_relaxed);
    float mix = mix_.load(std::memory_order_relaxed);

    for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
    {
        ma_uint32 readPos = (writePos_ + bufferSize - delaySamples_) % bufferSize;

        float delayed = buffer_[readPos];
        buffer_[writePos_] = pInput[i] + delayed * feedback;
        pOutput[i] = pInput[i] * (1.0f - mix) + delayed * mix;

        writePos_ = (writePos_ + 1) % bufferSize;
    }
}
