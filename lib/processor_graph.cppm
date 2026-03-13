module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>

export module processor_graph;

import node_base;

export class ProcessorGraph
{
public:
    ProcessorGraph(ma_uint32 channels);

    ~ProcessorGraph();

    // movable
    ProcessorGraph(ProcessorGraph&& other) noexcept;
    ProcessorGraph& operator=(ProcessorGraph&& other) noexcept;

    ma_node_graph* get();
    ma_node* getEndpoint();
    ma_uint32 getChannels() const;
    ma_uint32 read(float* output, ma_uint32 frameCount);
    void connect(NodeBase* from, NodeBase* to);
    void connectToOutput(NodeBase* node);

private:
    ma_node_graph graph_;
    ma_uint32 channels_;
    bool initialized_;
};

ProcessorGraph::ProcessorGraph(ma_uint32 channels)
    : channels_(channels), initialized_(false)
{
    ma_node_graph_config config = ma_node_graph_config_init(channels);

    ma_result result = ma_node_graph_init(&config, nullptr, &graph_);

    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(std::string("Failed to initialize graph: ") + ma_result_description(result));
    }

    initialized_ = true;
}

ProcessorGraph::~ProcessorGraph()
{
    if (initialized_)
    {
        ma_node_graph_uninit(&graph_, nullptr);
    }
}

ProcessorGraph::ProcessorGraph(ProcessorGraph&& other) noexcept
    : graph_(other.graph_), channels_(other.channels_), initialized_(other.initialized_)
{
    other.initialized_ = false;
}

ProcessorGraph& ProcessorGraph::operator=(ProcessorGraph&& other) noexcept
{
    if (this != &other)
    {
        if (initialized_)
        {
            ma_node_graph_uninit(&graph_, nullptr);
        }
        graph_ = other.graph_;
        channels_ = other.channels_;
        initialized_ = other.initialized_;
        other.initialized_ = false;
    }
    return *this;
}

ma_node_graph* ProcessorGraph::get()
{
    return &graph_;
}

ma_node* ProcessorGraph::getEndpoint()
{
    return ma_node_graph_get_endpoint(&graph_);
}

ma_uint32 ProcessorGraph::getChannels() const
{
    return channels_;
}

ma_uint32 ProcessorGraph::read(float* output, ma_uint32 frameCount)
{
    ma_uint64 framesRead;
    ma_node_graph_read_pcm_frames(&graph_, output, frameCount, &framesRead);

    return static_cast<ma_uint32>(framesRead);
}

void ProcessorGraph::connect(NodeBase* from, NodeBase* to)
{
    ma_node_attach_output_bus(from->getNode(), 0, to->getNode(), 0);
}

void ProcessorGraph::connectToOutput(NodeBase* node)
{
    ma_node_attach_output_bus(node->getNode(), 0, ma_node_graph_get_endpoint(&graph_), 0);
}
