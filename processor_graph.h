#include "miniaudio.h"

class ProcessorGraph
{
public:
    ProcessorGraph(
        ma_uint32 channels,
        ma_uint32 sampleRate = 44100
    );

    ~ProcessorGraph();

    // movable
    ProcessorGraph(ProcessorGraph&& other) noexcept;
    ProcessorGraph& operator=(ProcessorGraph&& other) noexcept;

    ma_node_graph* get();
    const ma_node_graph* get() const;
    ma_node* getEndpoint();
    ma_uint32 read(float* output, ma_uint32 frameCount);
private:
    ma_node_graph graph_;
    ma_uint32 channels_;
    ma_uint32 sampleRate_;
    bool initialized_;
};
