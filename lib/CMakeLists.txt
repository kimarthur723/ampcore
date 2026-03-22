add_library(amp_core STATIC)
target_sources(amp_core PUBLIC FILE_SET CXX_MODULES FILES
  node_base.cppm
  processor_graph.cppm
  audio_engine.cppm
  source_node.cppm
  effect_node.cppm
  nodes/file_input_node.cppm
  nodes/fuzz.cppm
  nodes/delay.cppm
  nodes/primitives.cppm
)
set_property(TARGET amp_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(amp_core PUBLIC miniaudio_headers PRIVATE pthread m dl)

add_library(amp_capi SHARED amp_capi.cpp)
target_include_directories(amp_capi PUBLIC .)
target_link_libraries(amp_capi PRIVATE amp_core)
