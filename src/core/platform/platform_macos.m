#include "platform.h"

#if defined(PLATFORM_MACOS)
#include "../array/darray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>

@class WindowDelegate;
@class CanvasView;

typedef struct platform_state {
  vwindow **windows;
  platform_window_resize_callback window_resize_callback;
  platform_window_render_callback window_render_callback;
  platform_window_close_callback window_close_callback;
} platform_state;

typedef struct platform_window_state {
  NSWindow *handle;
  CanvasView *view;
  WindowDelegate *delegate;
  b8 is_closed;
} platform_window_state;

static platform_state *state_ptr;

@interface CanvasView : NSView
@property(nonatomic, assign) vwindow *handle;
@end

@implementation CanvasView

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    self.wantsLayer = YES;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    self.layer.contentsGravity = kCAGravityTopLeft;
  }
  return self;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)isOpaque {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
  [[NSColor blackColor] setFill];
  NSRectFill(dirtyRect);

  if (state_ptr->window_render_callback) {
    state_ptr->window_render_callback(self.handle);
  }
}

@end

@interface WindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) vwindow *handle;
@end

@implementation WindowDelegate
- (void)windowDidResize:(NSNotification *)notification {
  NSWindow *window = notification.object;
  NSRect frame = [window contentView].frame;

  self.handle->width = (u32)frame.size.width;
  self.handle->height = (u32)frame.size.height;

  if (state_ptr && state_ptr->window_resize_callback) {
    state_ptr->window_resize_callback(self.handle, self.handle->width,
                                      self.handle->height);
  }
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
  return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
  b8 is_last_window = (darray_length(state_ptr->windows) <= 1);

  if (state_ptr && state_ptr->window_close_callback) {
    state_ptr->window_close_callback(self.handle, is_last_window);
  }
  self.handle->internal_handle->is_closed = true;
}
@end

b8 platform_initalize() {
  NSApplication *application = [NSApplication sharedApplication];

  [application setActivationPolicy:NSApplicationActivationPolicyRegular];
  [application activateIgnoringOtherApps:YES];

  state_ptr = malloc(sizeof(platform_state));

  if (!state_ptr) {
    return false;
  }

  state_ptr->windows = darray_create(vwindow *, 1);
  state_ptr->window_render_callback = PNULL;
  state_ptr->window_resize_callback = PNULL;
  state_ptr->window_close_callback = PNULL;

  return true;
}

b8 platform_window_create(vwindow *out_handle, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y) {
  if (!state_ptr) {
    return false;
  }

  platform_window_state *state = malloc(sizeof(platform_window_state));
  if (!state) {
    return false;
  }

  state->handle = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(x, y, w, h)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  if (!state->handle) {
    free(state);
    return false;
  }

  if (name) {
    [state->handle setTitle:[NSString stringWithUTF8String:name]];
  }

  state->view = [[CanvasView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
  state->handle.contentView = state->view;
  state->view.handle = out_handle;

  state->delegate = [[WindowDelegate alloc] init];
  if (!state->delegate) {
    free(state);
    return false;
  }

  state->delegate.handle = out_handle;
  [state->handle setDelegate:state->delegate];

  [state->handle makeKeyAndOrderFront:nil];
  state->is_closed = false;
  out_handle->internal_handle = state;
  darray_push(state_ptr->windows, out_handle);

  return true;
}

void platform_window_destroy(vwindow *handle) {
  if (!handle || !handle->internal_handle)
    return;

  platform_window_state *state =
      (platform_window_state *)handle->internal_handle;
  if (state->is_closed)
    return;
  [state->handle close];

  [state->handle setDelegate:nil];
  state->delegate = nil;

  free(state);
  handle->internal_handle = PNULL;
}
static vwindow *vwindow_from_NSWindow(NSWindow *handle,
                                                  u64 *out_index) {
  for (u64 i = 0; i < darray_length(state_ptr->windows); i++) {
    if (state_ptr->windows[i]->internal_handle->handle == handle) {
      if (out_index) {
        *out_index = i;
      }
      return state_ptr->windows[i];
    }
  }
  return 0;
}

typedef struct platform_graphics_context_state {
  vwindow *window;
} platform_graphics_context_state;

b8 platform_graphics_context_create(graphics_context_handle *out_context,
                                    vwindow *window) {
  platform_window_state *window_state =
      (platform_window_state *)window->internal_handle;

  platform_graphics_context_state *state =
      malloc(sizeof(platform_graphics_context_state));

  if (!state) {
    return false;
  }

  state->window = window;
  out_context->internal_handle = state;

  return true;
}

void platform_graphics_context_put_image(graphics_context_handle *context,
                                         bitmap bm, u32 x, u32 y) {
  // TODO COME BACK TO THIS
  CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
  if (!ctx)
    return;

  size_t bytes_per_row = (size_t)bm.width * sizeof(uint32_t);
  size_t data_size = bytes_per_row * bm.height;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider =
      CGDataProviderCreateWithData(PNULL, bm.pixels, data_size, PNULL);

  CGImageRef cgImage = CGImageCreate(
      bm.width, bm.height, 8, 32, bytes_per_row, colorSpace,
      kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider,
      PNULL, false, kCGRenderingIntentDefault);

  CGRect rect =
      CGRectMake((CGFloat)x, (CGFloat)y, (CGFloat)bm.width, (CGFloat)bm.height);
  CGContextDrawImage(ctx, rect, cgImage);

  CGImageRelease(cgImage);
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(colorSpace);
}

b8 platform_pump_message() {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    NSEvent *event = PNULL;
    do {
      event = [app nextEventMatchingMask:NSEventMaskAny
                               untilDate:PNULL
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:true];

      [app sendEvent:event];
      [app updateWindows];
    } while (event != PNULL);
  }
  return true;
}

void platform_window_set_resize_callback(
    platform_window_resize_callback callback) {
  state_ptr->window_resize_callback = callback;
}

void platform_window_set_render_callback(
    platform_window_render_callback callback) {
  state_ptr->window_render_callback = callback;
}

void platform_window_set_close_callback(
    platform_window_close_callback callback) {
  state_ptr->window_close_callback = callback;
}
#endif
