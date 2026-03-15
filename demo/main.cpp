#include <iostream>
#include <stdexcept>

import processor_graph;
import audio_engine;
import file_input_node;
import fuzz;
import delay;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <wav_file>" << std::endl;
    return 1;
  }
  const char *wavFile = argv[1];
  try {
    ProcessorGraph graph(2);
    AudioEngine engine(graph);
    auto sampleRate = engine.getSampleRate();

    FileInputNode fileInput(graph, wavFile, graph.getChannels(), sampleRate);
    Fuzz fuzz(graph, graph.getChannels(), 10.0f, 0.3f);
    Delay delay(graph, graph.getChannels(), sampleRate, 0.5f, 0.4f, 0.5f);

    // initial chain: FileInput -> Fuzz -> Output (delay bypassed)
    graph.connect(&fileInput, &fuzz);
    graph.connectToOutput(&fuzz);

    engine.start();

    std::cout << "Playing (fuzz only). Press Enter to toggle delay, 'q' + Enter to quit." << std::endl;

    bool delayActive = false;
    std::string line;
    while (std::getline(std::cin, line)) {
      if (line == "q") break;

      if (!delayActive) {
        // insert delay: fuzz -> delay -> output
        graph.postConnect(&fuzz, &delay);
        graph.postConnectToOutput(&delay);
        delayActive = true;
        std::cout << "delay on" << std::endl;
      } else {
        // bypass delay: fuzz -> output
        graph.postConnectToOutput(&fuzz);
        graph.postDisconnect(&delay);
        delayActive = false;
        std::cout << "delay off" << std::endl;
      }
    }

    engine.stop();
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
