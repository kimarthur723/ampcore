#pragma once
#include "effect_node.h"

class Fuzz : public EffectNode
{
public:
    Fuzz(ma_node_graph* graph, ma_uint32 channels, float gain = 10.0f, float threshold = 0.3f);

    void setGain(float gain) { gain_ = gain; }
    void setThreshold(float threshold) { threshold_ = threshold; }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    float gain_;
    float threshold_;
    ma_uint32 channels_;
};
