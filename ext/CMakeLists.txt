include(FetchContent)

set(GLFW_STANDALONE ON)

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

FetchContent_MakeAvailable(glfw nanovg spdlog glew glm freetype)

add_library(nanovg)
target_sources(nanovg PRIVATE ${CMAKE_BINARY_DIR}/_deps/nanovg-src/src/nanovg.c)
target_include_directories(nanovg PUBLIC ${CMAKE_BINARY_DIR}/_deps/nanovg-src/src)
target_compile_definitions(nanovg PRIVATE FONS_USE_FREETYPE)
target_link_libraries(nanovg PRIVATE freetype)
