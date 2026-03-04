#pragma once
#include "miniaudio.h"
#include "node_base.h"

class ProcessorGraph
{
public:
    ProcessorGraph(ma_uint32 channels);

    ~ProcessorGraph();

    // movable
    ProcessorGraph(ProcessorGraph&& other) noexcept;
    ProcessorGraph& operator=(ProcessorGraph&& other) noexcept;

    ma_node_graph* get();
    ma_node* getEndpoint();
    ma_uint32 read(float* output, ma_uint32 frameCount);
    void connectToOutput(NodeBase* node);
private:
    ma_node_graph graph_;
    bool initialized_;
};
