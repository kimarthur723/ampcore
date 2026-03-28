module;
#include "miniaudio.h"
#include <atomic>
#include <cmath>
#include <complex>
#include <mutex>
#include <vector>

export module cabinet;

import node_base;
import processor_graph;
import effect_node;

namespace detail {

// Radix-2 Cooley-Tukey FFT (in-place, iterative)
// inverse=false for forward FFT, inverse=true for IFFT
inline void fft(std::vector<std::complex<float>>& x, bool inverse)
{
    std::size_t N = x.size();
    if (N <= 1) return;

    // Bit-reversal permutation
    for (std::size_t i = 1, j = 0; i < N; ++i)
    {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(x[i], x[j]);
    }

    // Butterfly stages
    for (std::size_t len = 2; len <= N; len <<= 1)
    {
        float angle = (inverse ? 2.0f : -2.0f) *
                      static_cast<float>(M_PI) / static_cast<float>(len);
        std::complex<float> wlen(std::cos(angle), std::sin(angle));

        for (std::size_t i = 0; i < N; i += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            for (std::size_t j = 0; j < len / 2; ++j)
            {
                std::complex<float> u = x[i + j];
                std::complex<float> v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        float invN = 1.0f / static_cast<float>(N);
        for (auto& val : x)
            val *= invN;
    }
}

// Round up to next power of two
inline std::size_t nextPow2(std::size_t v)
{
    std::size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

} // namespace detail

export class CabinetNode : public EffectNode
{
public:
    CabinetNode(ProcessorGraph& graph, ma_uint32 channels);

    bool loadIR(const char* wavPath, ma_uint32 sampleRate);

    void setMix(float mix) { mix_.store(mix, std::memory_order_relaxed); }

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    int getParameterCount() const override { return 1; }
    ParameterInfo getParameterInfo(int index) const override
    {
        switch (index) {
            case 0: return {"mix", 0.0f, 1.0f, 1.0f, ""};
            default: return {"", 0, 0, 0, ""};
        }
    }
    float getParameterValue(int index) const override
    {
        switch (index) {
            case 0: return mix_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }
    void setParameterValue(int index, float value) override
    {
        switch (index) {
            case 0: mix_.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    ma_uint32 channels_;
    std::atomic<float> mix_{1.0f};

    // Mutex to protect IR state between loadIR (main thread) and process (audio thread)
    std::mutex irMutex_;

    // IR state — set by loadIR, read by process
    bool irLoaded_{false};
    std::size_t fftSize_{0};
    std::size_t irLen_{0};
    std::vector<float> irTimeDomain_;              // original time-domain IR samples
    std::vector<std::complex<float>> irSpectrum_;

    // Pre-allocated work buffer for process() — avoids per-call heap allocation
    std::vector<std::complex<float>> freqBuf_;

    // Overlap-add state (per channel)
    std::vector<std::vector<float>> overlapBuf_;  // tail from previous block
    std::size_t blockSize_{0};                     // expected frameCount
};

CabinetNode::CabinetNode(ProcessorGraph& graph, ma_uint32 channels)
    : EffectNode(graph, channels)
    , channels_(channels)
{
}

bool CabinetNode::loadIR(const char* wavPath, ma_uint32 sampleRate)
{
    ma_decoder_config decoderConfig = ma_decoder_config_init(
        ma_format_f32, 1, sampleRate);

    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(wavPath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS)
        return false;

    // Read the entire IR into a buffer
    // Assume reasonable IR length (up to 10 seconds)
    constexpr ma_uint64 maxFrames = 480000;
    std::vector<float> irData(maxFrames);

    ma_uint64 framesRead = 0;
    result = ma_decoder_read_pcm_frames(&decoder, irData.data(), maxFrames, &framesRead);
    ma_decoder_uninit(&decoder);

    if (framesRead == 0)
        return false;

    irData.resize(static_cast<std::size_t>(framesRead));

    // Prepare all new state before taking the lock
    std::size_t irLen = irData.size();
    std::size_t blockSize = 4096;
    std::size_t fftSize = detail::nextPow2(blockSize + irLen);

    // Pre-compute IR spectrum
    std::vector<std::complex<float>> irSpectrum(fftSize, {0.0f, 0.0f});
    for (std::size_t i = 0; i < irLen; ++i)
        irSpectrum[i] = {irData[i], 0.0f};
    detail::fft(irSpectrum, false);

    // Pre-allocate work buffer
    std::vector<std::complex<float>> freqBuf(fftSize);

    // Initialize overlap buffers (one per channel)
    std::vector<std::vector<float>> overlapBuf(channels_);
    for (auto& buf : overlapBuf)
        buf.assign(fftSize, 0.0f);

    // Swap all state under the lock
    {
        std::lock_guard<std::mutex> lock(irMutex_);
        irLen_ = irLen;
        blockSize_ = blockSize;
        fftSize_ = fftSize;
        irTimeDomain_ = std::move(irData);
        irSpectrum_ = std::move(irSpectrum);
        freqBuf_ = std::move(freqBuf);
        overlapBuf_ = std::move(overlapBuf);
        irLoaded_ = true;
    }

    return true;
}

void CabinetNode::process(float* pOutput, const float* pInput,
                           ma_uint32 frameCount)
{
    float mix = mix_.load(std::memory_order_relaxed);

    // Try to acquire lock; if loadIR() holds it, pass through to avoid blocking audio thread
    std::unique_lock<std::mutex> lock(irMutex_, std::try_to_lock);
    if (!lock.owns_lock() || !irLoaded_)
    {
        for (ma_uint32 i = 0; i < frameCount * channels_; ++i)
            pOutput[i] = pInput[i];
        return;
    }

    // If block size changed, recompute FFT size and IR spectrum from stored time-domain IR
    if (static_cast<std::size_t>(frameCount) != blockSize_)
    {
        blockSize_ = frameCount;
        std::size_t newFftSize = detail::nextPow2(blockSize_ + irLen_);
        if (newFftSize != fftSize_)
        {
            fftSize_ = newFftSize;

            // Re-FFT from the stored time-domain IR
            irSpectrum_.assign(fftSize_, {0.0f, 0.0f});
            for (std::size_t i = 0; i < irLen_; ++i)
                irSpectrum_[i] = {irTimeDomain_[i], 0.0f};
            detail::fft(irSpectrum_, false);

            // Resize pre-allocated work buffer
            freqBuf_.resize(fftSize_);

            for (auto& buf : overlapBuf_)
                buf.assign(fftSize_, 0.0f);
        }
    }

    // Process each channel independently via overlap-add
    for (ma_uint32 ch = 0; ch < channels_; ++ch)
    {
        // De-interleave input into zero-padded buffer
        for (std::size_t i = 0; i < fftSize_; ++i)
            freqBuf_[i] = {0.0f, 0.0f};

        for (ma_uint32 i = 0; i < frameCount; ++i)
            freqBuf_[i] = {pInput[i * channels_ + ch], 0.0f};

        // Forward FFT
        detail::fft(freqBuf_, false);

        // Complex multiply with IR spectrum
        for (std::size_t i = 0; i < fftSize_; ++i)
            freqBuf_[i] *= irSpectrum_[i];

        // Inverse FFT
        detail::fft(freqBuf_, true);

        // Add overlap from previous block and write output
        for (ma_uint32 i = 0; i < frameCount; ++i)
        {
            float wet = freqBuf_[i].real() + overlapBuf_[ch][i];
            float dry = pInput[i * channels_ + ch];
            pOutput[i * channels_ + ch] = dry * (1.0f - mix) + wet * mix;
        }

        // Save new overlap tail
        std::size_t overlapLen = fftSize_ - blockSize_;
        for (std::size_t i = 0; i < overlapLen; ++i)
        {
            if (i + blockSize_ < fftSize_)
                overlapBuf_[ch][i] = freqBuf_[i + blockSize_].real();
            else
                overlapBuf_[ch][i] = 0.0f;
        }
        // Zero the rest
        for (std::size_t i = overlapLen; i < fftSize_; ++i)
            overlapBuf_[ch][i] = 0.0f;
    }
}
