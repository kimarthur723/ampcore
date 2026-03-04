#pragma once
#include "source_node.h"

class FileInputNode : public SourceNode
{
public:
    FileInputNode(ma_node_graph* graph, const char* filePath,
                  ma_uint32 channels, ma_uint32 sampleRate);
    ~FileInputNode();

    void process(float* pOutput, const float* pInput,
                 ma_uint32 frameCount) override;

private:
    ma_decoder decoder_;
    bool decoderInitialized_;
};
