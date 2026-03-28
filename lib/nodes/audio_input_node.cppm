module;
#include "miniaudio.h"
#include <cstring>

export module audio_input_node;

import processor_graph;
import source_node;

export class AudioInputNode : public SourceNode
{
public:
    AudioInputNode(ProcessorGraph& graph, ma_uint32 channels);

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

    void writeInput(const float* pInput, ma_uint32 frameCount);
    void clearInput();

private:
    const float* inputPtr_ = nullptr;
    ma_uint32 inputFrameCount_ = 0;
    ma_uint32 channels_;
};

AudioInputNode::AudioInputNode(ProcessorGraph& graph, ma_uint32 channels)
    : SourceNode(graph, channels), channels_(channels)
{
}

void AudioInputNode::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    (void)pInput;

    if (inputPtr_ != nullptr)
    {
        ma_uint32 framesToCopy = (frameCount < inputFrameCount_) ? frameCount : inputFrameCount_;
        std::memcpy(pOutput, inputPtr_, framesToCopy * channels_ * sizeof(float));

        if (framesToCopy < frameCount)
        {
            std::memset(pOutput + framesToCopy * channels_, 0,
                        (frameCount - framesToCopy) * channels_ * sizeof(float));
        }
    }
    else
    {
        std::memset(pOutput, 0, frameCount * channels_ * sizeof(float));
    }
}

void AudioInputNode::writeInput(const float* pInput, ma_uint32 frameCount)
{
    inputPtr_ = pInput;
    inputFrameCount_ = frameCount;
}

void AudioInputNode::clearInput()
{
    inputPtr_ = nullptr;
    inputFrameCount_ = 0;
}
