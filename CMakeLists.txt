cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Signalbox C CXX)

file(GLOB_RECURSE Source src/*.cpp)

add_executable(Signalbox ${Source})

add_subdirectory(ext)
target_link_libraries(Signalbox PRIVATE glfw nanovg spdlog::spdlog libglew_static glm miniaudio SPSCQueue RingBuffer)
target_include_directories(Signalbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(Signalbox PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
