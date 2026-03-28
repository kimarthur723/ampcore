module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <vector>

export module primitives;

import node_base;
import processor_graph;
import effect_node;

// ── Gain ──────────────────────────────────────────────────────────────────────

export class Gain : public EffectNode
{
public:
    Gain(ProcessorGraph& graph, ma_uint32 channels, float gain = 1.0f)
        : EffectNode(graph, channels), channels_(channels), gain_(gain)
    {}

    void setGain(float gain) { gain_.store(gain, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput, ma_uint32 frameCount) override
    {
        float g = gain_.load(std::memory_order_relaxed);
        for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
            pOutput[i] = pInput[i] * g;
    }

    int getParameterCount() const override { return 1; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"gain", 0.0f, 10.0f, 1.0f, "x"};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return gain_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: gain_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    ma_uint32          channels_;
    std::atomic<float> gain_;
};

// ── Biquad ────────────────────────────────────────────────────────────────────
//
// Direct Form I two-pole two-zero filter.
// Coefficients derived from the RBJ Audio EQ Cookbook.
//
// Parameters are set from the GUI thread via atomics. The dirty_ flag signals
// the audio thread to recompute coefficients once at the top of the next
// process() call — no lock needed because only the audio thread writes the
// coefficient fields.

export class Biquad : public EffectNode
{
public:
    enum class Mode { LowPass, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf };

    Biquad(ProcessorGraph& graph, ma_uint32 channels, ma_uint32 sampleRate,
           Mode mode = Mode::LowPass, float freq = 1000.0f,
           float q = 0.707f, float dbGain = 0.0f)
        : EffectNode(graph, channels)
        , channels_(channels)
        , sampleRate_(sampleRate)
        , mode_(static_cast<int>(mode))
        , freq_(freq), q_(q), dbGain_(dbGain)
        , dirty_(true)
        , state_(static_cast<std::size_t>(channels) * 4, 0.0f)
        , b0_(1.0f), b1_(0.0f), b2_(0.0f), a1_(0.0f), a2_(0.0f)
    {}

    void setFreq(float freq) {
        freq_.store(freq, std::memory_order_relaxed);
        dirty_.store(true, std::memory_order_release);
    }
    void setQ(float q) {
        q_.store(q, std::memory_order_relaxed);
        dirty_.store(true, std::memory_order_release);
    }
    void setDbGain(float db) {
        dbGain_.store(db, std::memory_order_relaxed);
        dirty_.store(true, std::memory_order_release);
    }
    void setMode(Mode mode) {
        mode_.store(static_cast<int>(mode), std::memory_order_relaxed);
        dirty_.store(true, std::memory_order_release);
    }

    void process(float* pOutput, const float* pInput, ma_uint32 frameCount) override;

    int getParameterCount() const override { return 3; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"freq", 20.0f, 20000.0f, 1000.0f, "Hz"};
            case 1: return {"q", 0.1f, 20.0f, 0.707f, ""};
            case 2: return {"dbGain", -24.0f, 24.0f, 0.0f, "dB"};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return freq_.load(std::memory_order_relaxed);
            case 1: return q_.load(std::memory_order_relaxed);
            case 2: return dbGain_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: setFreq(value); break;
            case 1: setQ(value); break;
            case 2: setDbGain(value); break;
            default: break;
        }
    }

private:
    void recalcCoeffs();

    ma_uint32 channels_;
    ma_uint32 sampleRate_;

    std::atomic<int>   mode_;
    std::atomic<float> freq_;
    std::atomic<float> q_;
    std::atomic<float> dbGain_;
    std::atomic<bool>  dirty_;

    std::vector<float> state_; // [x1, x2, y1, y2] per channel

    // Written only by audio thread after dirty_ check — no sync needed.
    float b0_, b1_, b2_, a1_, a2_;
};

void Biquad::recalcCoeffs()
{
    constexpr float pi = 3.14159265358979323846f;

    float freq   = freq_.load(std::memory_order_relaxed);
    float q      = q_.load(std::memory_order_relaxed);
    float dbGain = dbGain_.load(std::memory_order_relaxed);
    int   mode   = mode_.load(std::memory_order_relaxed);

    float w0    = 2.0f * pi * freq / static_cast<float>(sampleRate_);
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / (2.0f * q);

    float A   = std::pow(10.0f, dbGain / 40.0f);
    float sqA = std::sqrt(A);

    float b0, b1, b2, a0, a1, a2;

    switch (static_cast<Mode>(mode)) {
        case Mode::LowPass:
            b0 = (1.0f - cosw0) * 0.5f;
            b1 =  1.0f - cosw0;
            b2 = (1.0f - cosw0) * 0.5f;
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha;
            break;
        case Mode::HighPass:
            b0 =  (1.0f + cosw0) * 0.5f;
            b1 = -(1.0f + cosw0);
            b2 =  (1.0f + cosw0) * 0.5f;
            a0 =   1.0f + alpha;
            a1 =  -2.0f * cosw0;
            a2 =   1.0f - alpha;
            break;
        case Mode::BandPass:
            b0 =  sinw0 * 0.5f;
            b1 =  0.0f;
            b2 = -sinw0 * 0.5f;
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha;
            break;
        case Mode::Notch:
            b0 =  1.0f;
            b1 = -2.0f * cosw0;
            b2 =  1.0f;
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha;
            break;
        case Mode::Peak:
            b0 =  1.0f + alpha * A;
            b1 = -2.0f * cosw0;
            b2 =  1.0f - alpha * A;
            a0 =  1.0f + alpha / A;
            a1 = -2.0f * cosw0;
            a2 =  1.0f - alpha / A;
            break;
        case Mode::LowShelf: {
            float inner = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
            float alphaS = sinw0 * 0.5f * std::sqrt(std::max(0.0f, inner));
            b0 =    A * ((A+1) - (A-1)*cosw0 + 2*sqA*alphaS);
            b1 = 2*A * ((A-1) - (A+1)*cosw0);
            b2 =    A * ((A+1) - (A-1)*cosw0 - 2*sqA*alphaS);
            a0 =        ((A+1) + (A-1)*cosw0 + 2*sqA*alphaS);
            a1 =   -2 * ((A-1) + (A+1)*cosw0);
            a2 =        ((A+1) + (A-1)*cosw0 - 2*sqA*alphaS);
            break;
        }
        case Mode::HighShelf: {
            float inner = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
            float alphaS = sinw0 * 0.5f * std::sqrt(std::max(0.0f, inner));
            b0 =    A * ((A+1) + (A-1)*cosw0 + 2*sqA*alphaS);
            b1 = -2*A * ((A-1) + (A+1)*cosw0);
            b2 =    A * ((A+1) + (A-1)*cosw0 - 2*sqA*alphaS);
            a0 =        ((A+1) - (A-1)*cosw0 + 2*sqA*alphaS);
            a1 =    2 * ((A-1) - (A+1)*cosw0);
            a2 =        ((A+1) - (A-1)*cosw0 - 2*sqA*alphaS);
            break;
        }
    }

    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}

void Biquad::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    if (dirty_.load(std::memory_order_acquire)) {
        recalcCoeffs();
        dirty_.store(false, std::memory_order_relaxed);
    }

    for (ma_uint32 ch = 0; ch < channels_; ++ch) {
        float x1 = state_[ch * 4 + 0];
        float x2 = state_[ch * 4 + 1];
        float y1 = state_[ch * 4 + 2];
        float y2 = state_[ch * 4 + 3];

        for (ma_uint32 f = 0; f < frameCount; ++f) {
            ma_uint32 i = f * channels_ + ch;
            float x = pInput[i];
            float y = b0_ * x + b1_ * x1 + b2_ * x2 - a1_ * y1 - a2_ * y2;
            pOutput[i] = y;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
        }

        state_[ch * 4 + 0] = x1;
        state_[ch * 4 + 1] = x2;
        state_[ch * 4 + 2] = y1;
        state_[ch * 4 + 3] = y2;
    }
}
