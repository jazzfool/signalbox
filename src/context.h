#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SB_USE_METAL
struct Context_MTL;
#endif

struct GLFWwindow;
struct NVGcontext;

typedef struct Context {
    struct GLFWwindow* window;
    struct NVGcontext* nvg;
#ifdef SB_USE_METAL
    struct Context_MTL* mtl;
#endif
} Context;

Context create_context();
void destroy_context(Context* cx);

void context_begin_frame(Context* cx, float r, float g, float b);
void context_end_frame(Context* cx);
void context_on_resize(Context* cx);

#ifdef __cplusplus
}
#endif
