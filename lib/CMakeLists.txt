add_library(amp_core STATIC)
target_sources(amp_core PUBLIC FILE_SET CXX_MODULES FILES
  node_base.cppm
  processor_graph.cppm
  audio_engine.cppm
  source_node.cppm
  effect_node.cppm
  nodes/file_input_node.cppm
  nodes/fuzz.cppm
)
target_link_libraries(amp_core PUBLIC miniaudio_headers PRIVATE pthread m dl)
