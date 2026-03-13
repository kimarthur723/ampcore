#include <iostream>
#include <stdexcept>

import processor_graph;
import audio_engine;
import file_input_node;
import fuzz;

int main(int argc, char *argv[]) {
  const char *wavFile = argc > 1 ? argv[1] : "assets/test.wav";
  try {
    ProcessorGraph graph(2);
    AudioEngine engine(graph);
    ma_uint32 sampleRate = engine.getSampleRate();

    FileInputNode fileInput(graph, wavFile, graph.getChannels(), sampleRate);
    Fuzz fuzz(graph, graph.getChannels(), 10.0f, 0.3f);

    // FileInput -> Fuzz -> Output
    graph.connect(&fileInput, &fuzz);
    graph.connectToOutput(&fuzz);

    engine.start();

    std::cout << "Playing file with fuzz..." << std::endl;
    std::cin.get();

    engine.stop();
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
