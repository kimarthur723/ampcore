module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>

export module effect_node;

import node_base;
import processor_graph;

export class EffectNode : public NodeBase
{
public:
    EffectNode(ProcessorGraph& graph, ma_uint32 channels);
    ~EffectNode();
};

static void effectProcessCallback(ma_node* pNode, const float** ppFramesIn,
                                   ma_uint32* pFrameCountIn, float** ppFramesOut,
                                   ma_uint32* pFrameCountOut)
{
    (void)pFrameCountIn;

    NodeWrapper* wrapper = static_cast<NodeWrapper*>(pNode);
    wrapper->owner->process(ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable effectVTable = {
    effectProcessCallback,
    nullptr,
    1,
    1,
    0
};

EffectNode::EffectNode(ProcessorGraph& graph, ma_uint32 channels)
    : NodeBase(graph.get())
{
    ma_uint32 inputChannels[] = { channels };
    ma_uint32 outputChannels[] = { channels };

    ma_node_config config = ma_node_config_init();
    config.vtable = &effectVTable;
    config.pInputChannels = inputChannels;
    config.pOutputChannels = outputChannels;

    wrapper_.owner = this;

    ma_result result = ma_node_init(graph.get(), &config, nullptr, &wrapper_.base);

    if (result != MA_SUCCESS)
    {
        throw std::runtime_error(
            std::string("Failed to initialize effect node: ") +
            ma_result_description(result)
        );
    }
}

EffectNode::~EffectNode()
{
    ma_node_uninit(&wrapper_.base, nullptr);
}
