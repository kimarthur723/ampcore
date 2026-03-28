module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <vector>

export module tone_stack;

import node_base;
import processor_graph;
import effect_node;

// 3-band EQ using three biquad filters in series:
//   - Low shelf  at ~300 Hz
//   - Peaking    at ~800 Hz
//   - High shelf at ~3 kHz
//
// Each band's gain maps from 0..10 control range to -15..+15 dB.

export class ToneStack : public EffectNode
{
public:
    ToneStack(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
              float bass = 5.0f, float mid = 5.0f, float treble = 5.0f);

    void setBass(float value)   { bass_.store(value, std::memory_order_relaxed);
                                  dirty_.store(true, std::memory_order_release); }
    void setMid(float value)    { mid_.store(value, std::memory_order_relaxed);
                                  dirty_.store(true, std::memory_order_release); }
    void setTreble(float value) { treble_.store(value, std::memory_order_relaxed);
                                  dirty_.store(true, std::memory_order_release); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 3; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"bass", 0.0f, 10.0f, 5.0f, ""};
            case 1: return {"mid", 0.0f, 10.0f, 5.0f, ""};
            case 2: return {"treble", 0.0f, 10.0f, 5.0f, ""};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return bass_.load(std::memory_order_relaxed);
            case 1: return mid_.load(std::memory_order_relaxed);
            case 2: return treble_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: setBass(value); break;
            case 1: setMid(value); break;
            case 2: setTreble(value); break;
            default: break;
        }
    }

private:
    void recalcCoeffs();

    // Map 0..10 control to dB
    static float controlToDb(float v) { return (v - 5.0f) * 3.0f; }  // -15..+15 dB

    struct BiquadCoeffs {
        float b0, b1, b2, a1, a2;
    };

    void calcLowShelf(BiquadCoeffs& c, float freq, float dbGain, float q);
    void calcPeak(BiquadCoeffs& c, float freq, float dbGain, float q);
    void calcHighShelf(BiquadCoeffs& c, float freq, float dbGain, float q);

    ma_uint32 channels_;
    ma_uint32 sampleRate_;

    std::atomic<float> bass_;    // 0.0 to 10.0
    std::atomic<float> mid_;     // 0.0 to 10.0
    std::atomic<float> treble_;  // 0.0 to 10.0
    std::atomic<bool>  dirty_;

    BiquadCoeffs lowShelf_;
    BiquadCoeffs peak_;
    BiquadCoeffs highShelf_;

    // State: [x1, x2, y1, y2] per filter per channel (3 filters)
    std::vector<float> state_;  // channels * 3 * 4
};

ToneStack::ToneStack(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
                     float bass, float mid, float treble)
    : EffectNode(graph, channels)
    , channels_(channels)
    , sampleRate_(sampleRate)
    , bass_(bass)
    , mid_(mid)
    , treble_(treble)
    , dirty_(true)
    , state_(static_cast<std::size_t>(channels) * 3 * 4, 0.0f)
{
    recalcCoeffs();
}

void ToneStack::calcLowShelf(BiquadCoeffs& c, float freq, float dbGain, float q)
{
    constexpr float pi = 3.14159265358979323846f;
    float A = std::pow(10.0f, dbGain / 40.0f);
    float w0 = 2.0f * pi * freq / static_cast<float>(sampleRate_);
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float sqA = std::sqrt(A);
    float inner = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
    float alphaS = sinw0 * 0.5f * std::sqrt(std::max(0.0f, inner));

    float a0 =        (A+1) + (A-1)*cosw0 + 2*sqA*alphaS;
    c.b0 =    A *    ((A+1) - (A-1)*cosw0 + 2*sqA*alphaS) / a0;
    c.b1 = 2*A *     ((A-1) - (A+1)*cosw0)                / a0;
    c.b2 =    A *    ((A+1) - (A-1)*cosw0 - 2*sqA*alphaS) / a0;
    c.a1 =   -2 *    ((A-1) + (A+1)*cosw0)                / a0;
    c.a2 =           ((A+1) + (A-1)*cosw0 - 2*sqA*alphaS) / a0;
}

void ToneStack::calcPeak(BiquadCoeffs& c, float freq, float dbGain, float q)
{
    constexpr float pi = 3.14159265358979323846f;
    float A = std::pow(10.0f, dbGain / 40.0f);
    float w0 = 2.0f * pi * freq / static_cast<float>(sampleRate_);
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / (2.0f * q);

    float a0 = 1.0f + alpha / A;
    c.b0 = (1.0f + alpha * A) / a0;
    c.b1 = (-2.0f * cosw0)   / a0;
    c.b2 = (1.0f - alpha * A) / a0;
    c.a1 = (-2.0f * cosw0)   / a0;
    c.a2 = (1.0f - alpha / A) / a0;
}

void ToneStack::calcHighShelf(BiquadCoeffs& c, float freq, float dbGain, float q)
{
    constexpr float pi = 3.14159265358979323846f;
    float A = std::pow(10.0f, dbGain / 40.0f);
    float w0 = 2.0f * pi * freq / static_cast<float>(sampleRate_);
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float sqA = std::sqrt(A);
    float inner = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
    float alphaS = sinw0 * 0.5f * std::sqrt(std::max(0.0f, inner));

    float a0 =         (A+1) - (A-1)*cosw0 + 2*sqA*alphaS;
    c.b0 =    A *     ((A+1) + (A-1)*cosw0 + 2*sqA*alphaS) / a0;
    c.b1 = -2*A *     ((A-1) + (A+1)*cosw0)                / a0;
    c.b2 =    A *     ((A+1) + (A-1)*cosw0 - 2*sqA*alphaS) / a0;
    c.a1 =    2 *     ((A-1) - (A+1)*cosw0)                / a0;
    c.a2 =            ((A+1) - (A-1)*cosw0 - 2*sqA*alphaS) / a0;
}

void ToneStack::recalcCoeffs()
{
    float bass   = bass_.load(std::memory_order_relaxed);
    float mid    = mid_.load(std::memory_order_relaxed);
    float treble = treble_.load(std::memory_order_relaxed);

    calcLowShelf(lowShelf_,   300.0f,  controlToDb(bass),   0.707f);
    calcPeak(peak_,           800.0f,  controlToDb(mid),    0.707f);
    calcHighShelf(highShelf_, 3000.0f, controlToDb(treble), 0.707f);
}

void ToneStack::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    if (dirty_.load(std::memory_order_acquire)) {
        recalcCoeffs();
        dirty_.store(false, std::memory_order_relaxed);
    }

    const BiquadCoeffs* filters[3] = { &lowShelf_, &peak_, &highShelf_ };

    // Copy input to output first, then process each filter in-place
    for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
        pOutput[i] = pInput[i];

    for (int filt = 0; filt < 3; ++filt)
    {
        const BiquadCoeffs& co = *filters[filt];

        for (ma_uint32 ch = 0; ch < channels_; ++ch)
        {
            std::size_t stateBase = (static_cast<std::size_t>(ch) * 3 + filt) * 4;
            float x1 = state_[stateBase + 0];
            float x2 = state_[stateBase + 1];
            float y1 = state_[stateBase + 2];
            float y2 = state_[stateBase + 3];

            for (ma_uint32 f = 0; f < frameCount; ++f)
            {
                ma_uint32 i = f * channels_ + ch;
                float x = pOutput[i];
                float y = co.b0 * x + co.b1 * x1 + co.b2 * x2 - co.a1 * y1 - co.a2 * y2;
                pOutput[i] = y;
                x2 = x1; x1 = x;
                y2 = y1; y1 = y;
            }

            state_[stateBase + 0] = x1;
            state_[stateBase + 1] = x2;
            state_[stateBase + 2] = y1;
            state_[stateBase + 3] = y2;
        }
    }
}
