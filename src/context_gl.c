#include "context.h"

#ifndef SB_USE_METAL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

context create_context() {
    context cx;

    glfwInit();

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    cx.window = glfwCreateWindow(400, 300, "Signalbox", NULL, NULL);

    glfwMakeContextCurrent(cx.window);
    glfwSwapInterval(0);

    glewExperimental = GL_TRUE;
    glewInit();

    cx.nvg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);

    return cx;
}

void destroy_context(context* cx) {
    nvgDeleteGL3(cx->nvg);
    glfwDestroyWindow(cx->window);
    glfwTerminate();
}

void context_begin_frame(context* cx, float r, float g, float b) {
    int fb_width, fb_height;
    glfwGetFramebufferSize(cx->window, &fb_width, &fb_height);

    glViewport(0, 0, fb_width, fb_height);
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void context_end_frame(context* cx) {
    glfwSwapBuffers(cx->window);
}

void context_on_resize(context* cx) {
}

#endif
