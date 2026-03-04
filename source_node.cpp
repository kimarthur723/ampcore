#include "source_node.h"
#include <stdexcept>
#include <string>

static void sourceProcessCallback(ma_node* pNode, const float** ppFramesIn,
                                   ma_uint32* pFrameCountIn, float** ppFramesOut,
                                   ma_uint32* pFrameCountOut)
{
    (void)ppFramesIn;
    (void)pFrameCountIn;

    NodeWrapper* wrapper = static_cast<NodeWrapper*>(pNode);
    wrapper->owner->process(ppFramesOut[0], nullptr, *pFrameCountOut);
}

static ma_node_vtable sourceVTable = {
    sourceProcessCallback,
    nullptr,
    0,
    1,
    0
};

SourceNode::SourceNode(ma_node_graph* graph, ma_uint32 channels)
    : NodeBase(graph)
{
    ma_uint32 outputChannels[] = { channels };

    ma_node_config config = ma_node_config_init();
    config.vtable = &sourceVTable;
    config.pOutputChannels = outputChannels;

    wrapper_.owner = this;

    ma_result result = ma_node_init(graph, &config, nullptr, &wrapper_.base);

    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(
            std::string("Failed to initialize source node: ") +
            ma_result_description(result)
        );
    }
}

SourceNode::~SourceNode()
{
    ma_node_uninit(&wrapper_.base, nullptr);
}
