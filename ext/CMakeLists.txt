include(FetchContent)

set(GLFW_STANDALONE OFF)
set(RH_STANDALONE_PROJECT OFF)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 7482de6071d21db77a7236155da44c172a7f6c9e # v3.3.8
)

FetchContent_Declare(
    nanovg
    GIT_REPOSITORY https://github.com/memononen/nanovg.git
    GIT_TAG 7544c114e83db7cf67bd1c9e012349b70caacc2f # latest
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG ad0e89cbfb4d0c1ce4d097e134eb7be67baebb36 # v1.11.0
)

FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG 8260c05ee0121094a0dc9a53f610f514659763d6 # latest
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # v0.9.9.8
)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG e8ebfe988b5f57bfb9a3ecb13c70d9791bce9ecf # v2.12.1
)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG c153a947919808419b0bf3f56b6f2ee606d6c5f4 # latest
)

FetchContent_Declare(
    SPSCQueue
    GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
    GIT_TAG 565a5149d54930463d58cb0f69b978d439555e66 # v1.1
)

FetchContent_Declare(
    RingBuffer
    GIT_REPOSITORY https://github.com/jnk0le/Ring-Buffer.git
    GIT_TAG 9217a86f6bc2ed9fcdd20775aaaafab856ff98e2 # latest
)

FetchContent_Declare(
    muFFT
    GIT_REPOSITORY https://github.com/Themaister/muFFT.git
    GIT_TAG 47bb08652eab399c2c7d460abe5184857110f130 # latest
)

FetchContent_Declare(
    sse_mathfun
    GIT_REPOSITORY https://github.com/RJVB/sse_mathfun.git
    GIT_TAG 0f462645698320643decdb5f79d211733b2a97d6 # latest
)

FetchContent_Declare(
    sse2neon
    GIT_REPOSITORY https://github.com/DLTcollab/sse2neon.git
    GIT_TAG 31cb30b1f93256d7aa19bfbaf4558c1d35e9b3ba # latest
)

FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG 6efc824070c79afd42a3a9e08da8f867d0ca2a97 # v1.0.1
)

FetchContent_Declare(
    robin-hood
    GIT_REPOSITORY https://github.com/martinus/robin-hood-hashing.git
    GIT_TAG 9145f963d80d6a02f0f96a47758050a89184a3ed # v3.11.5
)

if(${SB_USE_METAL})
    FetchContent_Declare(
        MetalNanoVG
        GIT_REPOSITORY https://github.com/ollix/MetalNanoVG.git
        GIT_TAG 58670cd64715e99b00c2fbddb6ace2264873ce91 # latest
    )
endif()

FetchContent_MakeAvailable(glfw nanovg spdlog glew glm freetype miniaudio SPSCQueue RingBuffer muFFT sse_mathfun sse2neon nfd robin-hood)

add_library(nanovg)
target_sources(nanovg PRIVATE ${CMAKE_BINARY_DIR}/_deps/nanovg-src/src/nanovg.c)
target_include_directories(nanovg PUBLIC ${CMAKE_BINARY_DIR}/_deps/nanovg-src/src)
target_compile_definitions(nanovg PRIVATE FONS_USE_FREETYPE)
target_link_libraries(nanovg PRIVATE freetype)

if(${SB_USE_METAL})
    FetchContent_MakeAvailable(MetalNanoVG)

    add_library(MetalNanoVG)
    target_sources(MetalNanoVG PRIVATE ${CMAKE_BINARY_DIR}/_deps/metalnanovg-src/src/nanovg_mtl.m)
    target_include_directories(MetalNanoVG PUBLIC ${CMAKE_BINARY_DIR}/_deps/metalnanovg-src/src)
    target_link_libraries(MetalNanoVG PRIVATE nanovg "-framework Metal" "-framework QuartzCore")
endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_BINARY_DIR}/_deps/miniaudio-src)

add_library(RingBuffer INTERFACE)
target_include_directories(RingBuffer INTERFACE ${CMAKE_BINARY_DIR}/_deps/ringbuffer-src)

add_library(sse_mathfun INTERFACE)
target_include_directories(sse_mathfun INTERFACE ${CMAKE_BINARY_DIR}/_deps/sse_mathfun-src)

add_library(sse2neon INTERFACE)
target_include_directories(sse2neon INTERFACE ${CMAKE_BINARY_DIR}/_deps/sse2neon-src)
