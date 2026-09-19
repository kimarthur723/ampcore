module;
#include "miniaudio.h"

export module node_base;

export class NodeBase;

export struct NodeWrapper
{
    ma_node_base base;
    NodeBase* owner;
};

export struct ParameterInfo {
    const char* name;
    float min_value;
    float max_value;
    float default_value;
    const char* unit;
};

export class NodeBase
{
public:
    using DestroyHook = void (*)(void* ctx, NodeBase* node);

    NodeBase(ma_node_graph* graph) { (void)graph; }
    virtual ~NodeBase()
    {
        if (destroyHook_) destroyHook_(destroyCtx_, this);
    }

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

    virtual int getParameterCount() const { return 0; }
    virtual ParameterInfo getParameterInfo(int index) const { (void)index; return {"", 0, 0, 0, ""}; }
    virtual float getParameterValue(int index) const { (void)index; return 0.0f; }
    virtual void setParameterValue(int index, float value) { (void)index; (void)value; }

    void setDestroyHook(DestroyHook hook, void* ctx)
    {
        destroyHook_ = hook;
        destroyCtx_ = ctx;
    }

protected:
    NodeWrapper wrapper_;

private:
    DestroyHook destroyHook_ = nullptr;
    void* destroyCtx_ = nullptr;
};
