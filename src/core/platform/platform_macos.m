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

typedef struct vwindow_platform_state {
  NSWindow *window;
  CGImageRef window_client_buffer;
  CanvasView *view;
  WindowDelegate *delegate;
} vwindow_platform_state;

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

    // move origin to top left
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
  if (state_ptr && state_ptr->window_close_callback) {
    state_ptr->window_close_callback(self.handle);
  }

  return YES;
}
@end

b8 platform_initalize() {
  if (state_ptr) return false; // already initalized
  state_ptr = malloc(sizeof(platform_state));
  if (!state_ptr) {
    return false;
  }
  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    [application setActivationPolicy:NSApplicationActivationPolicyRegular];
    [application activateIgnoringOtherApps:YES];

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    
    state_ptr->windows = darray_create(vwindow *, 1);
    state_ptr->window_render_callback = PNULL;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
  }
  return true;
}

void platform_uninitalize(void)
{
    if (state_ptr) {
        u32 len = darray_length(state_ptr->windows);
        for (u32 i = 0; i < len; ++i) {
            if (state_ptr->windows[i] != PNULL) {
                //DestroyWindow(state_ptr->windows[i]->platform_state->window);
                state_ptr->windows[i]->platform_state->window = PNULL;
                free(state_ptr->windows[i]->platform_state);
                state_ptr->windows[i]->platform_state = PNULL;
                state_ptr->windows[i] = NULL;
            }
        }

        darray_destroy(state_ptr->windows);
        state_ptr->windows = PNULL;
        state_ptr->window_render_callback = PNULL;
        state_ptr->window_resize_callback = PNULL;
        state_ptr->window_close_callback = PNULL;
        free(state_ptr);
        state_ptr = PNULL;
    }
}

b8 platform_window_create(vwindow *out_handle, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y) {
  if (!state_ptr) {
    return false;
  }

  vwindow_platform_state *state = malloc(sizeof(vwindow_platform_state));
  if (!state) {
    return false;
  }
  @autoreleasepool {
  state->window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(x, y, w, h)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  if (!state->window) {
    free(state);
    return false;
  }

  if (name) {
    [state->window setTitle:[NSString stringWithUTF8String:name]];
  }

  state->delegate = [[WindowDelegate alloc] init];
  if (!state->delegate) {
    free(state);
    return false;
  }

  state->delegate.handle = out_handle;
  [state->window setDelegate:state->delegate];

  [state->window makeKeyAndOrderFront:nil];
  out_handle->platform_state = state;
  
  // get the size of the client area
  NSRect contentBounds = [state->window.contentView bounds];

  state->view = [[CanvasView alloc] initWithFrame:contentBounds];
  if(!state->view) {
    return false;
  }
  state->window.contentView = state->view;
  state->view.handle = out_handle;

  out_handle->width = (u32)contentBounds.size.width;
  out_handle->height = (u32)contentBounds.size.height;
  darray_push(state_ptr->windows, out_handle);
  }
  return true;
}

void platform_window_destroy(vwindow *window) {
  if (!window || !window->platform_state)
    return;
    
  @autoreleasepool {
    u32 const len = darray_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
      if (state_ptr->windows[i] == window) {
        vwindow_platform_state *plat_state = window->platform_state;
        
        // Clean up Objective-C objects before freeing state
        if (plat_state->window) {
          [plat_state->window setDelegate:nil];
          [plat_state->window close]; // Release window from screen
          plat_state->window = nil;
        }
        plat_state->delegate = nil;
        plat_state->view = nil;
        
        free(plat_state);
        window->platform_state = PNULL;
        state_ptr->windows[i] = PNULL; // this could break because old indexes could not be resued, though it breaks from many creates and destroys.
        //[0][NULL][2][3][4]...
        //   ^----freed window
        // In platform_window_create I call darray_push(state_ptr->windows, out_handle);
        return;
      }
    }
  }
}

static vwindow *vwindow_from_NSWindow(NSWindow *handle,
                                                  u64 *out_index) {
  for (u64 i = 0; i < darray_length(state_ptr->windows); i++) {
    if (state_ptr->windows[i]->platform_state->window == handle) {
      if (out_index) {
        *out_index = i;
      }
      return state_ptr->windows[i];
    }
  }
  return 0;
}

typedef struct vwindow_context_state {
  vwindow *window;
} vwindow_context_state;

b8 platform_graphics_context_create(vwindow_context *out_context,
                                    vwindow *window) {
  vwindow_platform_state *window_state =
      (vwindow_platform_state *)window->platform_state;

  vwindow_context_state *state =
      malloc(sizeof(vwindow_context_state));

  if (!state) {
    return false;
  }

  state->window = window;
  out_context->platform_state = state;

  return true;
}

void platform_graphics_context_put_image(vwindow_context *context,
                                         bitmap bm, u32 x, u32 y) {
  @autoreleasepool {
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
}

b8 platform_pump_message() {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    NSEvent *event = PNULL;
    do {
      event = [app nextEventMatchingMask:NSEventMaskAny
                               untilDate:nil
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:true];

      [app sendEvent:event];
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
