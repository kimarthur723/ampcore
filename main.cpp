#include "audio_engine.h"
#include "file_input_node.h"
#include "fuzz.h"
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        AudioEngine engine;
        auto& graph = engine.getGraph();
        ma_uint32 channels = engine.getChannels();
        ma_uint32 sampleRate = engine.getSampleRate();

        FileInputNode fileInput(graph.get(), "test.wav", channels, sampleRate);
        Fuzz fuzz(graph.get(), channels, 10.0f, 0.3f);

        // FileInput -> Fuzz -> Output
        ma_node_attach_output_bus(fileInput.getNode(), 0, fuzz.getNode(), 0);
        graph.connectToOutput(&fuzz);

        engine.start();

        std::cout << "Playing file with fuzz..." << std::endl;
        std::cin.get();

        engine.stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
