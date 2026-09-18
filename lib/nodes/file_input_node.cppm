module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>
#include <cstring>
#include <atomic>

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

    // when true, seeks back to the start on EOF instead of going silent
    void setLooping(bool loop) { looping_.store(loop, std::memory_order_relaxed); }
    bool isLooping() const { return looping_.load(std::memory_order_relaxed); }

private:
    ma_decoder decoder_;
    bool decoderInitialized_;
    std::atomic<bool> looping_{false};
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

    ma_uint32 channels = decoder_.outputChannels;
    ma_uint64 totalRead = 0;
    bool justWrapped = false;

    while (totalRead < frameCount)
    {
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(
            &decoder_, pOutput + totalRead * channels, frameCount - totalRead, &framesRead);
        totalRead += framesRead;

        if (framesRead > 0)
        {
            justWrapped = false;
            continue;
        }

        // EOF: wrap around if looping, otherwise leave the rest silent.
        // A zero-frame read right after a wrap means the file is empty —
        // bail rather than spin on the audio thread.
        if (!looping_.load(std::memory_order_relaxed) || justWrapped ||
            ma_decoder_seek_to_pcm_frame(&decoder_, 0) != MA_SUCCESS)
            break;

        justWrapped = true;
    }

    if (totalRead < frameCount)
    {
        std::memset(pOutput + totalRead * channels, 0,
                    (frameCount - totalRead) * channels * sizeof(float));
    }
}
