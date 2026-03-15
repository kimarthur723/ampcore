module;
#include "miniaudio.h"

export module fuzz;

import processor_graph;
import effect_node;

export class Fuzz : public EffectNode
{
public:
    Fuzz(ProcessorGraph& graph, ma_uint32 channels, float gain = 10.0f, float threshold = 0.3f);

    void setGain(float gain) { gain_ = gain; }
    void setThreshold(float threshold) { threshold_ = threshold; }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    float gain_;
    float threshold_;
    ma_uint32 channels_;
};

Fuzz::Fuzz(ProcessorGraph& graph, ma_uint32 channels, float gain, float threshold)
    : EffectNode(graph, channels), gain_(gain), threshold_(threshold), channels_(channels)
{
}

void Fuzz::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
    {
        float sample = pInput[i] * gain_;

        if (sample > threshold_)
            sample = threshold_;
        else if (sample < -threshold_)
            sample = -threshold_;

        pOutput[i] = sample;
    }
}
