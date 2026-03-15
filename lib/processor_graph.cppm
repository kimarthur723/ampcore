module;
#include "miniaudio.h"
#include <stdexcept>
#include <string>
#include <atomic>
#include <cstddef>

export module processor_graph;

import node_base;

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
    void postConnect(NodeBase* from, NodeBase* to);
    void postConnectToOutput(NodeBase* node);
    void postDisconnect(NodeBase* node); // detaches node without destroying it
    void postRemove(NodeBase* node);    // detaches node and queues it for collection

    // call from GUI thread to reclaim nodes removed via postRemove
    template<typename Fn>
    void collectGarbage(Fn&& fn)
    {
        NodeBase* node;
        while (gcQueue_.pop(node))
            fn(node);
    }

private:
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

void ProcessorGraph::postConnect(NodeBase* from, NodeBase* to)
{
    commandQueue_.push({ GraphCommand::Type::Connect, from, to });
}

void ProcessorGraph::postConnectToOutput(NodeBase* node)
{
    commandQueue_.push({ GraphCommand::Type::ConnectToOutput, node, nullptr });
}

void ProcessorGraph::postDisconnect(NodeBase* node)
{
    commandQueue_.push({ GraphCommand::Type::Disconnect, node, nullptr });
}

void ProcessorGraph::postRemove(NodeBase* node)
{
    commandQueue_.push({ GraphCommand::Type::Remove, node, nullptr });
}
