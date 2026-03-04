CXX = g++
CXXFLAGS = -Wall -Wextra -g
LIBS = -lpthread -lm -ldl

SRCS = main.cpp audio_engine.cpp processor_graph.cpp source_node.cpp \
       effect_node.cpp file_input_node.cpp fuzz.cpp

amp: $(SRCS)
	$(CXX) $(CXXFLAGS) -o amp $(SRCS) $(LIBS)

clean:
	rm -f amp
