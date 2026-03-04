#pragma once
#include "miniaudio.h"
#include "node_base.h"

class SourceNode: public NodeBase
{
public:
    SourceNode(ma_node_graph* graph, ma_uint32 channels);
    ~SourceNode();
};
