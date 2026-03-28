add_library(sigchain_core STATIC)
target_sources(sigchain_core PUBLIC FILE_SET CXX_MODULES FILES
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
set_property(TARGET sigchain_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(sigchain_core PUBLIC miniaudio_headers Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(sigchain_core PRIVATE m dl)
endif()

add_library(sigchain_capi SHARED sigchain_capi.cpp)
target_include_directories(sigchain_capi PUBLIC .)
target_link_libraries(sigchain_capi PRIVATE sigchain_core)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS sigchain_core sigchain_capi
    EXPORT sigchain-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES sigchain_capi.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sigchain)

install(EXPORT sigchain-targets
    FILE sigchainTargets.cmake
    NAMESPACE sigchain::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sigchain
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/sigchainConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sigchainConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sigchain
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sigchainConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SemVer
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sigchainConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sigchainConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sigchain
)
