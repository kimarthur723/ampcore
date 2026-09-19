module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>
#include <cstddef>

export module processor_graph;

import node_base;
import preset;

// lock-free single-producer single-consumer ring buffer
// N must be a power of 2
template<typename T, size_t N>
class SPSCQueue
{
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");

    T buffer_[N];
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};

public:
    bool push(const T& val)
    {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (N - 1);

        if (next == head_.load(std::memory_order_acquire))
            return false; // full

        buffer_[tail] = val;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& val)
    {
        size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_.load(std::memory_order_acquire))
            return false; // empty

        val = buffer_[head];
        head_.store((head + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool empty() const
    {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
};

struct GraphCommand
{
    enum class Type : uint8_t { Connect, ConnectToOutput, Disconnect, Remove };
    Type type;
    NodeBase* a;
    NodeBase* b; // null for ConnectToOutput and Remove
};

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

    // drains command queue then reads; called by audio thread
    ma_uint32 read(float* output, ma_uint32 frameCount);

    // immediate — safe to call before engine starts
    void connect(NodeBase* from, NodeBase* to);
    void connectToOutput(NodeBase* node);

    // thread-safe — enqueue mutations for the audio thread to apply
    bool postConnect(NodeBase* from, NodeBase* to);
    bool postConnectToOutput(NodeBase* node);
    bool postDisconnect(NodeBase* node); // detaches node without destroying it
    bool postRemove(NodeBase* node);    // detaches node and queues it for collection

    // node registry — registered nodes are serialized; connections between them
    // are recorded by connect/post* and dropped by postDisconnect/postRemove
    struct RegisteredNode {
        int id;
        std::string typeName;
        NodeBase* node;
    };

    int  registerNode(const std::string& typeName, NodeBase* node);
    void registerNode(int id, const std::string& typeName, NodeBase* node);
    void unregisterNode(int id);
    void unregisterNode(NodeBase* node);
    int  findNodeId(const NodeBase* node) const;
    const std::vector<RegisteredNode>& getRegisteredNodes() const { return registeredNodes_; }

    void registerConnection(int fromId, int toId, bool toOutput = false);

    bool hasPendingCommands() const { return !commandQueue_.empty(); }

    // serialization
    std::string serializeToJson() const;
    bool loadFromJson(const std::string& json,
                      NodeBase* (*createNode)(const std::string& type, ma_node_graph* graph),
                      void (*destroyNode)(NodeBase* node));

    // call from GUI thread to reclaim nodes removed via postRemove
    template<typename Fn>
    void collectGarbage(Fn&& fn)
    {
        NodeBase* node;
        while (gcQueue_.pop(node))
            fn(node);
    }

private:
    std::vector<RegisteredNode> registeredNodes_;
    int nextNodeId_ = 1;

    struct RegisteredConnection {
        int fromId;
        int toId;       // -1 means output
        bool toOutput;
    };
    std::vector<RegisteredConnection> registeredConnections_;
    ma_node_graph graph_;
    ma_uint32 channels_;
    bool initialized_;

    SPSCQueue<GraphCommand, 64> commandQueue_; // GUI -> audio
    SPSCQueue<NodeBase*, 64>    gcQueue_;      // audio -> GUI

    void drainCommands();
    void recordConnection(NodeBase* from, NodeBase* to, bool toOutput);
    void dropConnectionsFrom(NodeBase* node);
    void rebindDestroyHooks();
    static void onNodeDestroyed(void* ctx, NodeBase* node);
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
    : registeredNodes_(std::move(other.registeredNodes_)),
      nextNodeId_(other.nextNodeId_),
      registeredConnections_(std::move(other.registeredConnections_)),
      graph_(other.graph_), channels_(other.channels_), initialized_(other.initialized_)
{
    other.initialized_ = false;
    rebindDestroyHooks();
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
        registeredNodes_ = std::move(other.registeredNodes_);
        nextNodeId_ = other.nextNodeId_;
        registeredConnections_ = std::move(other.registeredConnections_);
        other.initialized_ = false;
        rebindDestroyHooks();
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

void ProcessorGraph::drainCommands()
{
    GraphCommand cmd;
    while (commandQueue_.pop(cmd))
    {
        switch (cmd.type)
        {
            case GraphCommand::Type::Connect:
                ma_node_attach_output_bus(cmd.a->getNode(), 0, cmd.b->getNode(), 0);
                break;

            case GraphCommand::Type::ConnectToOutput:
                ma_node_attach_output_bus(cmd.a->getNode(), 0, ma_node_graph_get_endpoint(&graph_), 0);
                break;

            case GraphCommand::Type::Disconnect:
                ma_node_detach_output_bus(cmd.a->getNode(), 0);
                break;

            case GraphCommand::Type::Remove:
                ma_node_detach_output_bus(cmd.a->getNode(), 0);
                gcQueue_.push(cmd.a);
                break;
        }
    }
}

ma_uint32 ProcessorGraph::read(float* output, ma_uint32 frameCount)
{
    drainCommands();

    ma_uint64 framesRead;
    ma_node_graph_read_pcm_frames(&graph_, output, frameCount, &framesRead);

    return static_cast<ma_uint32>(framesRead);
}

void ProcessorGraph::connect(NodeBase* from, NodeBase* to)
{
    ma_node_attach_output_bus(from->getNode(), 0, to->getNode(), 0);
    recordConnection(from, to, false);
}

void ProcessorGraph::connectToOutput(NodeBase* node)
{
    ma_node_attach_output_bus(node->getNode(), 0, ma_node_graph_get_endpoint(&graph_), 0);
    recordConnection(node, nullptr, true);
}

bool ProcessorGraph::postConnect(NodeBase* from, NodeBase* to)
{
    if (!commandQueue_.push({ GraphCommand::Type::Connect, from, to }))
        return false;
    recordConnection(from, to, false);
    return true;
}

bool ProcessorGraph::postConnectToOutput(NodeBase* node)
{
    if (!commandQueue_.push({ GraphCommand::Type::ConnectToOutput, node, nullptr }))
        return false;
    recordConnection(node, nullptr, true);
    return true;
}

bool ProcessorGraph::postDisconnect(NodeBase* node)
{
    if (!commandQueue_.push({ GraphCommand::Type::Disconnect, node, nullptr }))
        return false;
    dropConnectionsFrom(node);
    return true;
}

bool ProcessorGraph::postRemove(NodeBase* node)
{
    if (!commandQueue_.push({ GraphCommand::Type::Remove, node, nullptr }))
        return false;
    unregisterNode(node);
    return true;
}

// one output bus per node, so a new edge from `from` replaces the old one
void ProcessorGraph::recordConnection(NodeBase* from, NodeBase* to, bool toOutput)
{
    int fromId = findNodeId(from);
    if (fromId < 0) return;

    int toId = -1;
    if (!toOutput)
    {
        toId = findNodeId(to);
        if (toId < 0) return;
    }

    dropConnectionsFrom(from);
    registeredConnections_.push_back({fromId, toId, toOutput});
}

void ProcessorGraph::dropConnectionsFrom(NodeBase* node)
{
    int id = findNodeId(node);
    if (id < 0) return;
    std::erase_if(registeredConnections_,
                  [id](const RegisteredConnection& c) { return c.fromId == id; });
}

int ProcessorGraph::registerNode(const std::string& typeName, NodeBase* node)
{
    int id = nextNodeId_++;
    registerNode(id, typeName, node);
    return id;
}

void ProcessorGraph::registerNode(int id, const std::string& typeName, NodeBase* node)
{
    registeredNodes_.push_back({id, typeName, node});
    if (id >= nextNodeId_) nextNodeId_ = id + 1;
    node->setDestroyHook(&ProcessorGraph::onNodeDestroyed, this);
}

void ProcessorGraph::unregisterNode(int id)
{
    for (auto& rn : registeredNodes_)
        if (rn.id == id) rn.node->setDestroyHook(nullptr, nullptr);
    std::erase_if(registeredNodes_,
                  [id](const RegisteredNode& rn) { return rn.id == id; });
    std::erase_if(registeredConnections_,
                  [id](const RegisteredConnection& c) { return c.fromId == id || c.toId == id; });
}

void ProcessorGraph::onNodeDestroyed(void* ctx, NodeBase* node)
{
    static_cast<ProcessorGraph*>(ctx)->unregisterNode(node);
}

void ProcessorGraph::rebindDestroyHooks()
{
    for (auto& rn : registeredNodes_)
        rn.node->setDestroyHook(&ProcessorGraph::onNodeDestroyed, this);
}

void ProcessorGraph::unregisterNode(NodeBase* node)
{
    int id = findNodeId(node);
    if (id >= 0) unregisterNode(id);
}

int ProcessorGraph::findNodeId(const NodeBase* node) const
{
    for (const auto& rn : registeredNodes_)
        if (rn.node == node) return rn.id;
    return -1;
}

void ProcessorGraph::registerConnection(int fromId, int toId, bool toOutput)
{
    registeredConnections_.push_back({fromId, toId, toOutput});
}

std::string ProcessorGraph::serializeToJson() const
{
    std::vector<PresetNode> nodes;
    for (const auto& rn : registeredNodes_)
    {
        PresetNode pn;
        pn.id = rn.id;
        pn.type = rn.typeName;

        int paramCount = rn.node->getParameterCount();
        for (int i = 0; i < paramCount; ++i)
        {
            ParameterInfo info = rn.node->getParameterInfo(i);
            float val = rn.node->getParameterValue(i);
            pn.params.push_back({info.name, val});
        }
        nodes.push_back(std::move(pn));
    }

    std::vector<PresetConnection> connections;
    for (const auto& rc : registeredConnections_)
    {
        PresetConnection pc;
        pc.from = rc.fromId;
        pc.to = rc.toId;
        pc.toOutput = rc.toOutput;
        connections.push_back(pc);
    }

    return serializePreset(nodes, connections);
}

bool ProcessorGraph::loadFromJson(const std::string& json,
                                  NodeBase* (*createNode)(const std::string& type, ma_node_graph* graph),
                                  void (*destroyNode)(NodeBase* node))
{
    std::vector<PresetNode> nodes;
    std::vector<PresetConnection> connections;
    if (!parsePreset(json, nodes, connections))
        return false;

    auto old = std::move(registeredNodes_);
    registeredNodes_.clear();
    registeredConnections_.clear();
    for (auto& rn : old)
    {
        rn.node->setDestroyHook(nullptr, nullptr);
        ma_node_detach_output_bus(rn.node->getNode(), 0);
        destroyNode(rn.node);
    }

    for (const auto& pn : nodes)
    {
        NodeBase* node = createNode(pn.type, &graph_);
        if (!node) return false;

        for (const auto& [name, value] : pn.params)
        {
            int paramCount = node->getParameterCount();
            for (int i = 0; i < paramCount; ++i)
            {
                if (std::string(node->getParameterInfo(i).name) == name)
                {
                    node->setParameterValue(i, value);
                    break;
                }
            }
        }

        registerNode(pn.id, pn.type, node);
    }

    for (const auto& pc : connections)
    {
        NodeBase* fromNode = nullptr;
        NodeBase* toNode = nullptr;

        for (const auto& rn : registeredNodes_)
        {
            if (rn.id == pc.from) fromNode = rn.node;
            if (rn.id == pc.to)   toNode = rn.node;
        }

        if (!fromNode) return false;

        if (pc.toOutput)
        {
            connectToOutput(fromNode);
        }
        else
        {
            if (!toNode) return false;
            connect(fromNode, toNode);
        }
    }

    return true;
}
