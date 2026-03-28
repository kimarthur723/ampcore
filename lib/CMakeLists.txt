add_library(ampcore_core STATIC)
target_sources(ampcore_core PUBLIC FILE_SET CXX_MODULES FILES
  node_base.cppm
  processor_graph.cppm
  audio_engine.cppm
  source_node.cppm
  effect_node.cppm
  preset.cppm
  nodes/file_input_node.cppm
  nodes/audio_input_node.cppm
  nodes/fuzz.cppm
  nodes/delay.cppm
  nodes/primitives.cppm
  nodes/noise_gate.cppm
  nodes/compressor.cppm
  nodes/reverb.cppm
  nodes/tone_stack.cppm
  nodes/cabinet.cppm
  nodes/meter_node.cppm
)
set_property(TARGET ampcore_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(ampcore_core PUBLIC miniaudio_headers Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(ampcore_core PRIVATE m dl)
endif()

add_library(ampcore_capi SHARED ampcore_capi.cpp)
target_include_directories(ampcore_capi PUBLIC .)
target_link_libraries(ampcore_capi PRIVATE ampcore_core)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ampcore_core ampcore_capi
    EXPORT ampcore-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ampcore_capi.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ampcore)

install(EXPORT ampcore-targets
    FILE ampcoreTargets.cmake
    NAMESPACE ampcore::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampcore
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ampcoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ampcoreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampcore
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ampcoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SemVer
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ampcoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ampcoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ampcore
)
