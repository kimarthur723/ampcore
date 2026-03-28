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

    // node registry — tracks nodes with type names for serialization
    void registerNode(int id, const std::string& typeName, NodeBase* node);
    void unregisterNode(int id);

    void registerConnection(int fromId, int toId, bool toOutput = false);

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
    struct RegisteredNode {
        int id;
        std::string typeName;
        NodeBase* node;
    };
    std::vector<RegisteredNode> registeredNodes_;

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
      registeredConnections_(std::move(other.registeredConnections_)),
      graph_(other.graph_), channels_(other.channels_), initialized_(other.initialized_)
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
        registeredNodes_ = std::move(other.registeredNodes_);
        registeredConnections_ = std::move(other.registeredConnections_);
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
}

void ProcessorGraph::connectToOutput(NodeBase* node)
{
    ma_node_attach_output_bus(node->getNode(), 0, ma_node_graph_get_endpoint(&graph_), 0);
}

bool ProcessorGraph::postConnect(NodeBase* from, NodeBase* to)
{
    return commandQueue_.push({ GraphCommand::Type::Connect, from, to });
}

bool ProcessorGraph::postConnectToOutput(NodeBase* node)
{
    return commandQueue_.push({ GraphCommand::Type::ConnectToOutput, node, nullptr });
}

bool ProcessorGraph::postDisconnect(NodeBase* node)
{
    return commandQueue_.push({ GraphCommand::Type::Disconnect, node, nullptr });
}

bool ProcessorGraph::postRemove(NodeBase* node)
{
    return commandQueue_.push({ GraphCommand::Type::Remove, node, nullptr });
}

void ProcessorGraph::registerNode(int id, const std::string& typeName, NodeBase* node)
{
    registeredNodes_.push_back({id, typeName, node});
}

void ProcessorGraph::unregisterNode(int id)
{
    for (auto it = registeredNodes_.begin(); it != registeredNodes_.end(); ++it)
    {
        if (it->id == id)
        {
            registeredNodes_.erase(it);
            break;
        }
    }
    // also remove connections involving this node
    auto cit = registeredConnections_.begin();
    while (cit != registeredConnections_.end())
    {
        if (cit->fromId == id || cit->toId == id)
            cit = registeredConnections_.erase(cit);
        else
            ++cit;
    }
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

    // tear down existing nodes
    for (auto& rn : registeredNodes_)
    {
        ma_node_detach_output_bus(rn.node->getNode(), 0);
        destroyNode(rn.node);
    }
    registeredNodes_.clear();
    registeredConnections_.clear();

    // create nodes
    for (const auto& pn : nodes)
    {
        NodeBase* node = createNode(pn.type, &graph_);
        if (!node) return false;

        // restore parameters
        for (const auto& [name, value] : pn.params)
        {
            int paramCount = node->getParameterCount();
            for (int i = 0; i < paramCount; ++i)
            {
                ParameterInfo info = node->getParameterInfo(i);
                if (std::string(info.name) == name)
                {
                    node->setParameterValue(i, value);
                    break;
                }
            }
        }

        registeredNodes_.push_back({pn.id, pn.type, node});
    }

    // rebuild connections
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

        registeredConnections_.push_back({pc.from, pc.to, pc.toOutput});
    }

    return true;
}
