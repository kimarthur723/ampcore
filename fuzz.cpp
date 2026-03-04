#include "fuzz.h"

Fuzz::Fuzz(ma_node_graph* graph, ma_uint32 channels, float gain, float threshold)
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
