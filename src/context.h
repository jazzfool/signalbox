#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SB_USE_METAL
struct context_mtl;
#endif

struct GLFWwindow;
struct NVGcontext;

typedef struct context {
    struct GLFWwindow* window;
    struct NVGcontext* nvg;
#ifdef SB_USE_METAL
    struct context_mtl* mtl;
#endif
} context;

context create_context();
void destroy_context(context* cx);

void context_begin_frame(context* cx, float r, float g, float b);
void context_end_frame(context* cx);
void context_on_resize(context* cx);

#ifdef __cplusplus
}
#endif
