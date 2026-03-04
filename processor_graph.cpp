#include "processor_graph.h"
#include <stdexcept>
#include <string>

ProcessorGraph::ProcessorGraph(ma_uint32 channels)
    : initialized_(false)
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
    : graph_(other.graph_), initialized_(other.initialized_)
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

ma_uint32 ProcessorGraph::read(float* output, ma_uint32 frameCount)
{
    ma_uint64 framesRead;
    ma_node_graph_read_pcm_frames(&graph_, output, frameCount, &framesRead);

    return static_cast<ma_uint32>(framesRead);
}

void ProcessorGraph::connectToOutput(NodeBase* node)
{
    ma_node_attach_output_bus(node->getNode(), 0, ma_node_graph_get_endpoint(&graph_), 0);
}
