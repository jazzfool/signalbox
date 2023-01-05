#include "context.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <nanovg.h>
#include <nanovg_mtl.h>

struct context_mtl {
	id<MTLDevice> device;
	CAMetalLayer* layer;
};

context create_context() {
	context cx;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	cx.window = glfwCreateWindow(400, 300, "Signalbox", NULL, NULL);

	cx.mtl = calloc(1, sizeof(struct context_mtl));

	NSWindow* nswin = glfwGetCocoaWindow(cx.window);
	cx.mtl->device = MTLCreateSystemDefaultDevice();
	cx.mtl->layer = [CAMetalLayer layer];
    cx.mtl->layer.device = cx.mtl->device;
    cx.mtl->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = cx.mtl->layer;
    nswin.contentView.wantsLayer = YES;

    cx.nvg = nvgCreateMTL(cx.mtl->layer, NVG_STENCIL_STROKES | NVG_ANTIALIAS);

    context_on_resize(&cx);

	return cx;
}

void destroy_context(context* cx) {
	free(cx->mtl);

	nvgDeleteMTL(cx->nvg);
	glfwDestroyWindow(cx->window);
	glfwTerminate();
}

void context_begin_frame(context* cx, float r, float g, float b) {
	mnvgClearWithColor(cx->nvg, nvgRGBf(r, g, b));
}

void context_end_frame(context* cx) {}

void context_on_resize(context* cx) {
	int fb_width, fb_height;
    glfwGetFramebufferSize(cx->window, &fb_width, &fb_height);

    CGSize sz;
    sz.width = fb_width;
    sz.height = fb_height;

	cx->mtl->layer.drawableSize = sz;
}
