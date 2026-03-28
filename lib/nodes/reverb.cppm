module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <vector>

export module reverb;

import node_base;
import processor_graph;
import effect_node;

// Freeverb-style reverb with 8 comb filters and 4 allpass filters.

namespace {

class CombFilter
{
public:
    CombFilter() = default;

    void init(int size)
    {
        buffer_.resize(size, 0.0f);
        bufferSize_ = size;
        index_ = 0;
        filterStore_ = 0.0f;
    }

    float process(float input, float feedback, float damp1, float damp2)
    {
        float output = buffer_[index_];
        filterStore_ = output * damp2 + filterStore_ * damp1;
        buffer_[index_] = input + filterStore_ * feedback;
        if (++index_ >= bufferSize_)
            index_ = 0;
        return output;
    }

private:
    std::vector<float> buffer_;
    int bufferSize_ = 0;
    int index_ = 0;
    float filterStore_ = 0.0f;
};

class AllpassFilter
{
public:
    AllpassFilter() = default;

    void init(int size)
    {
        buffer_.resize(size, 0.0f);
        bufferSize_ = size;
        index_ = 0;
    }

    float process(float input)
    {
        float bufOut = buffer_[index_];
        float output = -input + bufOut;
        buffer_[index_] = input + bufOut * 0.5f;
        if (++index_ >= bufferSize_)
            index_ = 0;
        return output;
    }

private:
    std::vector<float> buffer_;
    int bufferSize_ = 0;
    int index_ = 0;
};

// Classic Freeverb tuning constants (for 44100 Hz)
constexpr int combTunings[8]    = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
constexpr int allpassTunings[4] = { 556, 441, 341, 225 };

} // namespace

export class Reverb : public EffectNode
{
public:
    Reverb(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
           float roomSize = 0.5f, float damping = 0.5f, float mix = 0.3f);

    void setRoomSize(float size) { roomSize_.store(size, std::memory_order_relaxed); }
    void setDamping(float damp)  { damping_.store(damp, std::memory_order_relaxed); }
    void setMix(float mix)       { mix_.store(mix, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 3; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"roomSize", 0.0f, 1.0f, 0.5f, ""};
            case 1: return {"damping", 0.0f, 1.0f, 0.5f, ""};
            case 2: return {"mix", 0.0f, 1.0f, 0.3f, ""};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return roomSize_.load(std::memory_order_relaxed);
            case 1: return damping_.load(std::memory_order_relaxed);
            case 2: return mix_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: roomSize_.store(value, std::memory_order_relaxed); break;
            case 1: damping_.store(value, std::memory_order_relaxed); break;
            case 2: mix_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    ma_uint32 channels_;
    ma_uint32 sampleRate_;
    std::atomic<float> roomSize_;  // 0.0 to 1.0
    std::atomic<float> damping_;   // 0.0 to 1.0
    std::atomic<float> mix_;       // 0.0 to 1.0

    // Per-channel filter banks
    struct ChannelState {
        CombFilter    combs[8];
        AllpassFilter allpasses[4];
    };
    std::vector<ChannelState> channelStates_;
};

Reverb::Reverb(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
               float roomSize, float damping, float mix)
    : EffectNode(graph, channels)
    , channels_(channels)
    , sampleRate_(sampleRate)
    , roomSize_(roomSize)
    , damping_(damping)
    , mix_(mix)
    , channelStates_(channels)
{
    float srScale = static_cast<float>(sampleRate_) / 44100.0f;

    for (ma_uint32 ch = 0; ch < channels; ++ch)
    {
        // Spread stereo slightly by offsetting right channel
        int spread = (ch > 0) ? 23 : 0;

        for (int i = 0; i < 8; ++i)
        {
            int size = static_cast<int>(std::round((combTunings[i] + spread) * srScale));
            channelStates_[ch].combs[i].init(size);
        }

        for (int i = 0; i < 4; ++i)
        {
            int size = static_cast<int>(std::round((allpassTunings[i] + spread) * srScale));
            channelStates_[ch].allpasses[i].init(size);
        }
    }
}

void Reverb::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    float roomSize = roomSize_.load(std::memory_order_relaxed);
    float damping  = damping_.load(std::memory_order_relaxed);
    float mix      = mix_.load(std::memory_order_relaxed);

    // Freeverb scaling
    constexpr float roomScale  = 0.28f;
    constexpr float roomOffset = 0.7f;
    float feedback = roomSize * roomScale + roomOffset;
    float damp1    = damping;
    float damp2    = 1.0f - damping;
    constexpr float fixedGain = 0.015f;

    for (ma_uint32 f = 0; f < frameCount; ++f)
    {
        for (ma_uint32 ch = 0; ch < channels_; ++ch)
        {
            ma_uint32 i = f * channels_ + ch;
            float input = pInput[i] * fixedGain;
            float wet = 0.0f;

            // Sum 8 parallel comb filters
            for (int c = 0; c < 8; ++c)
                wet += channelStates_[ch].combs[c].process(input, feedback, damp1, damp2);

            // Pass through 4 series allpass filters
            for (int a = 0; a < 4; ++a)
                wet = channelStates_[ch].allpasses[a].process(wet);

            pOutput[i] = pInput[i] * (1.0f - mix) + wet * mix;
        }
    }
}
