module;
#include "miniaudio.h"

export module node_base;

export class NodeBase;

export struct NodeWrapper
{
    ma_node_base base;
    NodeBase* owner;
};

export class NodeBase
{
public:
    NodeBase(ma_node_graph* graph) { (void)graph; }
    virtual ~NodeBase() = default;

    // non-copyable
    NodeBase(const NodeBase&) = delete;
    NodeBase& operator=(const NodeBase&) = delete;

    // non-movable
    NodeBase(NodeBase&&) = delete;
    NodeBase& operator=(NodeBase&&) = delete;

    ma_node_base* getNode()
    {
        return &wrapper_.base;
    }

    virtual void process(float* pOutput,
                         const float* pInput,
                         ma_uint32 frameCount) = 0;
protected:
    NodeWrapper wrapper_;
};
