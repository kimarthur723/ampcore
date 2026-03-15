module;
#include "miniaudio.h"
#include <vector>

export module delay;

import processor_graph;
import effect_node;

export class Delay : public EffectNode
{
public:
    Delay(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
          float delaySeconds = 0.5f, float feedback = 0.4f, float mix = 0.5f);

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    std::vector<float> buffer_;
    ma_uint32 writePos_;
    ma_uint32 delaySamples_;
    ma_uint32 channels_;
    float feedback_;
    float mix_;
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

    for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
    {
        ma_uint32 readPos = (writePos_ + bufferSize - delaySamples_) % bufferSize;

        float delayed = buffer_[readPos];
        buffer_[writePos_] = pInput[i] + delayed * feedback_;
        pOutput[i] = pInput[i] * (1.0f - mix_) + delayed * mix_;

        writePos_ = (writePos_ + 1) % bufferSize;
    }
}
