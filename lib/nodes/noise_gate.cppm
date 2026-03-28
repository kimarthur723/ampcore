module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <vector>

export module noise_gate;

import node_base;
import processor_graph;
import effect_node;

export class NoiseGate : public EffectNode
{
public:
    NoiseGate(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
              float thresholdDb = -40.0f, float attack = 0.001f, float release = 0.1f);

    void setThreshold(float db) { threshold_.store(db, std::memory_order_relaxed); }
    void setAttack(float seconds) { attack_.store(seconds, std::memory_order_relaxed); }
    void setRelease(float seconds) { release_.store(seconds, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 3; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"threshold", -80.0f, 0.0f, -40.0f, "dB"};
            case 1: return {"attack", 0.0001f, 0.1f, 0.001f, "s"};
            case 2: return {"release", 0.001f, 1.0f, 0.1f, "s"};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return threshold_.load(std::memory_order_relaxed);
            case 1: return attack_.load(std::memory_order_relaxed);
            case 2: return release_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: threshold_.store(value, std::memory_order_relaxed); break;
            case 1: attack_.store(value, std::memory_order_relaxed); break;
            case 2: release_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    ma_uint32 channels_;
    ma_uint32 sampleRate_;
    std::atomic<float> threshold_;  // dB, -80 to 0
    std::atomic<float> attack_;     // seconds, 0.0001 to 0.1
    std::atomic<float> release_;    // seconds, 0.001 to 1.0
    std::vector<float> envelope_;   // per-channel envelope follower state
    std::vector<float> gateGain_;   // per-channel smoothed gate gain
};

NoiseGate::NoiseGate(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
                     float thresholdDb, float attack, float release)
    : EffectNode(graph, channels)
    , channels_(channels)
    , sampleRate_(sampleRate)
    , threshold_(thresholdDb)
    , attack_(attack)
    , release_(release)
    , envelope_(channels, 0.0f)
    , gateGain_(channels, 0.0f)
{
}

void NoiseGate::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    float thresholdDb = threshold_.load(std::memory_order_relaxed);
    float attackSec   = attack_.load(std::memory_order_relaxed);
    float releaseSec  = release_.load(std::memory_order_relaxed);

    float thresholdLin = std::pow(10.0f, thresholdDb / 20.0f);
    float sr = static_cast<float>(sampleRate_);
    float attackCoeff  = std::exp(-1.0f / (attackSec * sr));
    float releaseCoeff = std::exp(-1.0f / (releaseSec * sr));

    for (ma_uint32 f = 0; f < frameCount; ++f)
    {
        for (ma_uint32 ch = 0; ch < channels_; ++ch)
        {
            ma_uint32 i = f * channels_ + ch;
            float absVal = std::fabs(pInput[i]);

            // Envelope follower
            float env = envelope_[ch];
            if (absVal > env)
                env = absVal + attackCoeff * (env - absVal);
            else
                env = absVal + releaseCoeff * (env - absVal);
            envelope_[ch] = env;

            // Gate gain: open (1.0) when above threshold, close (0.0) when below
            float targetGain = (env >= thresholdLin) ? 1.0f : 0.0f;

            // Smooth the gate gain with attack/release
            float gain = gateGain_[ch];
            if (targetGain > gain)
                gain = targetGain + attackCoeff * (gain - targetGain);
            else
                gain = targetGain + releaseCoeff * (gain - targetGain);
            gateGain_[ch] = gain;

            pOutput[i] = pInput[i] * gain;
        }
    }
}
