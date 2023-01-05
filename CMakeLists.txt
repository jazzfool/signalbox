cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Signalbox C CXX OBJC)

set(Source
    src/board.cpp
    src/impl.cpp
    src/main.cpp
    src/space.cpp
    src/widget.cpp
    src/context_gl.c

    src/filters/chn.cpp
    src/filters/executor.cpp
    src/filters/fir.cpp
    src/filters/gen.cpp
    src/filters/sbfft.cpp
    src/filters/sse.c
    src/filters/tfm.cpp
    src/filters/viz.cpp
    src/filters/trk.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

if (${APPLE})
    set(SB_USE_METAL ON)
endif()

if (${SB_USE_METAL})
    set(Source ${Source} src/context_mtl.m)
endif()

add_executable(Signalbox ${Source})

add_subdirectory(ext)

target_link_libraries(Signalbox PRIVATE glfw nanovg spdlog::spdlog libglew_static glm miniaudio SPSCQueue RingBuffer muFFT sse_mathfun sse2neon)
if (${SB_USE_METAL})
    target_link_libraries(Signalbox PRIVATE MetalNanoVG)
    target_compile_definitions(Signalbox PRIVATE SB_USE_METAL)
endif()

target_include_directories(Signalbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(Signalbox PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
