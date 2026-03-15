module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>
#include <cstring>

export module file_input_node;

import processor_graph;
import source_node;

export class FileInputNode : public SourceNode
{
public:
    FileInputNode(ProcessorGraph& graph, const char* filePath,
                  ma_uint32 channels, ma_uint32 sampleRate);
    ~FileInputNode();

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    ma_decoder decoder_;
    bool decoderInitialized_;
};

FileInputNode::FileInputNode(ProcessorGraph& graph, const char* filePath,
                             ma_uint32 channels, ma_uint32 sampleRate)
    : SourceNode(graph, channels), decoderInitialized_(false)
{
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, channels, sampleRate);

    ma_result result = ma_decoder_init_file(filePath, &decoderConfig, &decoder_);

    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(
            std::string("Failed to open audio file: ") +
            ma_result_description(result)
        );
    }

    decoderInitialized_ = true;
}

FileInputNode::~FileInputNode()
{
    if (decoderInitialized_)
    {
        ma_decoder_uninit(&decoder_);
    }
}

void FileInputNode::process(float* pOutput, const float* pInput, ma_uint32 frameCount)
{
    (void)pInput;

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&decoder_, pOutput, frameCount, &framesRead);

    if (framesRead < frameCount)
    {
        ma_uint32 channels = decoder_.outputChannels;
        std::memset(pOutput + framesRead * channels, 0,
                    (frameCount - framesRead) * channels * sizeof(float));
    }
}
