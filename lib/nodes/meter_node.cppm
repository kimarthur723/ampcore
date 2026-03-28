module;
#include "miniaudio.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

export module meter_node;

import processor_graph;
import effect_node;

inline constexpr ma_uint32 kMaxMeterChannels = 8;

export class MeterNode : public EffectNode
{
public:
    MeterNode(ProcessorGraph& graph, ma_uint32 channels);

    float getRMS(ma_uint32 channel) const {
        if (channel >= channels_) return 0.0f;
        return rms_[channel].load(std::memory_order_relaxed);
    }
    float getPeak(ma_uint32 channel) const {
        if (channel >= channels_) return 0.0f;
        return peak_[channel].load(std::memory_order_relaxed);
    }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    ma_uint32 channels_;
    std::array<std::atomic<float>, kMaxMeterChannels> rms_{};
    std::array<std::atomic<float>, kMaxMeterChannels> peak_{};
};

MeterNode::MeterNode(ProcessorGraph& graph, ma_uint32 channels)
    : EffectNode(graph, channels), channels_(channels)
{
    for (ma_uint32 ch = 0; ch < kMaxMeterChannels; ++ch) {
        rms_[ch].store(0.0f, std::memory_order_relaxed);
        peak_[ch].store(0.0f, std::memory_order_relaxed);
    }
}

void MeterNode::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    ma_uint32 totalSamples = frameCount * channels_;

    std::memcpy(pOutput, pInput, totalSamples * sizeof(float));

    if (frameCount == 0) return;

    for (ma_uint32 ch = 0; ch < channels_; ++ch)
    {
        float sumSquares = 0.0f;
        float maxAbs = 0.0f;

        for (ma_uint32 f = 0; f < frameCount; ++f)
        {
            float s = pInput[f * channels_ + ch];
            sumSquares += s * s;
            float a = std::fabs(s);
            if (a > maxAbs)
                maxAbs = a;
        }

        rms_[ch].store(std::sqrt(sumSquares / static_cast<float>(frameCount)),
                       std::memory_order_relaxed);
        peak_[ch].store(maxAbs, std::memory_order_relaxed);
    }
}
