#pragma once
#include "node_base.h"

class EffectNode : public NodeBase
{
public:
    EffectNode(ma_node_graph* graph, ma_uint32 channels);
    ~EffectNode();
};
