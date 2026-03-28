module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <vector>

export module compressor;

import node_base;
import processor_graph;
import effect_node;

export class Compressor : public EffectNode
{
public:
    Compressor(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
               float thresholdDb = -20.0f, float ratio = 4.0f,
               float attack = 0.01f, float release = 0.1f, float makeupGainDb = 0.0f);

    void setThreshold(float db)     { threshold_.store(db, std::memory_order_relaxed); }
    void setRatio(float ratio)      { ratio_.store(ratio, std::memory_order_relaxed); }
    void setAttack(float seconds)   { attack_.store(seconds, std::memory_order_relaxed); }
    void setRelease(float seconds)  { release_.store(seconds, std::memory_order_relaxed); }
    void setMakeupGain(float db)    { makeupGain_.store(db, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 5; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"threshold", -60.0f, 0.0f, -20.0f, "dB"};
            case 1: return {"ratio", 1.0f, 20.0f, 4.0f, ""};
            case 2: return {"attack", 0.0001f, 0.3f, 0.01f, "s"};
            case 3: return {"release", 0.01f, 1.0f, 0.1f, "s"};
            case 4: return {"makeupGain", 0.0f, 40.0f, 0.0f, "dB"};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return threshold_.load(std::memory_order_relaxed);
            case 1: return ratio_.load(std::memory_order_relaxed);
            case 2: return attack_.load(std::memory_order_relaxed);
            case 3: return release_.load(std::memory_order_relaxed);
            case 4: return makeupGain_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: threshold_.store(value, std::memory_order_relaxed); break;
            case 1: ratio_.store(value, std::memory_order_relaxed); break;
            case 2: attack_.store(value, std::memory_order_relaxed); break;
            case 3: release_.store(value, std::memory_order_relaxed); break;
            case 4: makeupGain_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    ma_uint32 channels_;
    ma_uint32 sampleRate_;
    std::atomic<float> threshold_;   // dB, -60 to 0
    std::atomic<float> ratio_;       // 1.0 to 20.0
    std::atomic<float> attack_;      // seconds, 0.0001 to 0.3
    std::atomic<float> release_;     // seconds, 0.01 to 1.0
    std::atomic<float> makeupGain_;  // dB, 0 to 40
    std::vector<float> envelope_;    // per-channel envelope state (dB)
};

Compressor::Compressor(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
                       float thresholdDb, float ratio,
                       float attack, float release, float makeupGainDb)
    : EffectNode(graph, channels)
    , channels_(channels)
    , sampleRate_(sampleRate)
    , threshold_(thresholdDb)
    , ratio_(ratio)
    , attack_(attack)
    , release_(release)
    , makeupGain_(makeupGainDb)
    , envelope_(channels, -96.0f)
{
}

void Compressor::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    float threshDb   = threshold_.load(std::memory_order_relaxed);
    float ratio      = ratio_.load(std::memory_order_relaxed);
    float attackSec  = attack_.load(std::memory_order_relaxed);
    float releaseSec = release_.load(std::memory_order_relaxed);
    float makeupDb   = makeupGain_.load(std::memory_order_relaxed);

    float sr = static_cast<float>(sampleRate_);
    float attackCoeff  = std::exp(-1.0f / (attackSec * sr));
    float releaseCoeff = std::exp(-1.0f / (releaseSec * sr));
    float makeupLin    = std::pow(10.0f, makeupDb / 20.0f);

    for (ma_uint32 f = 0; f < frameCount; ++f)
    {
        for (ma_uint32 ch = 0; ch < channels_; ++ch)
        {
            ma_uint32 i = f * channels_ + ch;
            float x = pInput[i];

            // Convert to dB
            float absX = std::fabs(x);
            float xDb = (absX < 1e-8f) ? -160.0f : 20.0f * std::log10(absX);

            // Envelope follower in dB domain
            float env = envelope_[ch];
            if (xDb > env)
                env = xDb + attackCoeff * (env - xDb);
            else
                env = xDb + releaseCoeff * (env - xDb);
            envelope_[ch] = env;

            // Gain computation (feed-forward)
            float gainDb = 0.0f;
            if (env > threshDb)
                gainDb = (threshDb - env) * (1.0f - 1.0f / ratio);

            float gainLin = std::pow(10.0f, gainDb / 20.0f);
            pOutput[i] = x * gainLin * makeupLin;
        }
    }
}
