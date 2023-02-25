cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Signalbox C CXX)

set(Source
    src/app.cpp
    src/impl.cpp
    src/main.cpp
    src/ui.cpp
    src/context_gl.c
    src/enc.cpp
    src/draw.cpp
    src/tracker/tracker.cpp
)

file(GLOB_RECURSE Headers "src/*.h")
add_custom_target(headers SOURCES ${Headers})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -fexceptions")

if(${MSVC})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

if(${APPLE})
    set(SB_USE_METAL ON)
else()
    set(SB_USE_METAL OFF)
endif()

if(${SB_USE_METAL})
    enable_language(OBJC)
    set(Source ${Source} src/context_mtl.m)
endif()

add_executable(Signalbox ${Source})
set_property(TARGET Signalbox PROPERTY CXX_STANDARD 20)

add_subdirectory(ext)

target_link_libraries(
  Signalbox
  PRIVATE glfw
          nanovg
          spdlog::spdlog
          libglew_static
          glm
          miniaudio
          SPSCQueue
          RingBuffer
          muFFT
          sse_mathfun
          sse2neon
          nfd
          robin_hood
)

if(${SB_USE_METAL})
    target_link_libraries(Signalbox PRIVATE MetalNanoVG)
    target_compile_definitions(Signalbox PRIVATE SB_USE_METAL)
endif()

target_include_directories(Signalbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(Signalbox PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
                                             GLM_FORCE_CTOR_INIT)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
