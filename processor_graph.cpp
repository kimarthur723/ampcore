#include "processor_graph.h"

ProcessorGraph::ProcessorGraph(ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_node_graph_config config = ma_node_graph_config_init(channels);
    config.sampleRate = sampleRate;

    ma_result result = ma_node_graph_init(&config, nullptr, &graph_);
    
}
