cmake_minimum_required(VERSION 3.20)
project(neuronscope VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch FTXUI
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Add Executable
add_executable(neuronscope
  src/main.cpp
  src/tui/app.cpp
  src/tui/panels/topology_panel.cpp
  src/tui/panels/packet_stream_panel.cpp
  src/tui/panels/attention_panel.cpp
  src/tui/panels/metrics_panel.cpp
  src/tui/panels/anomaly_panel.cpp
  src/ipc/shared_memory_channel.cpp
  src/ipc/control_pipe.cpp
  src/model/transformer.cpp
)

target_include_directories(neuronscope PRIVATE src include)

target_link_libraries(neuronscope PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

# Add Tests Executable
add_executable(neuronscope_tests
  tests/test_all.cpp
  src/core/ring_buffer.h  # Include headers in target for IDEs
  src/core/model_topology.h
  src/core/anomaly_detector.h
)

target_include_directories(neuronscope_tests PRIVATE src include)


if(WIN32)
  target_compile_definitions(neuronscope PRIVATE NEURONSCOPE_WINDOWS)
else()
  target_compile_definitions(neuronscope PRIVATE NEURONSCOPE_UNIX)
  target_link_libraries(neuronscope PRIVATE rt pthread)
endif()
