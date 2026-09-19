// CODE FOR MACOS
#include "platform.h"

#if defined(PLATFORM_MACOS)
#include "../containers/vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <AppKit/AppKit.h>

@class WindowDelegate;
@class CanvasView;

typedef struct macos_handle_info{
    void* unused;
}macos_handle_info;

typedef struct platform_state {
    macos_handle_info* handle;
    vwindow **windows;
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
} platform_state;

typedef char* platform_string_internal;
typedef struct vwindow_platform_state {
  NSWindow *window;
  void* pixel_map;
  u32 pixel_width;
  u32 pixel_height;
  u32 pixel_x;
  u32 pixel_y;

  CanvasView *view;
  WindowDelegate *delegate;
} vwindow_platform_state;

static platform_state *state_ptr;

@interface CanvasView : NSView
@property(nonatomic, assign) vwindow *window;
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
  @autoreleasepool {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);
    if (!ctx)
      return;
  
    size_t bytes_per_row = (size_t)self.window->platform_state->pixel_width * sizeof(u32);
    size_t data_size = bytes_per_row * self.window->platform_state->pixel_height;
  
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider =
        CGDataProviderCreateWithData(PNULL, self.window->platform_state->pixel_map, data_size, PNULL);
  
    CGImageRef cgImage = CGImageCreate(
        self.window->platform_state->pixel_width, self.window->platform_state->pixel_height, 
        8, 32, bytes_per_row,
        colorSpace,
        kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, 
        provider,
        PNULL, false, kCGRenderingIntentDefault);
  
    CGRect rect = CGRectMake((CGFloat)self.window->platform_state->pixel_x, (CGFloat)self.window->platform_state->pixel_y, (CGFloat)self.window->platform_state->pixel_width, (CGFloat)self.window->platform_state->pixel_height);
    CGContextDrawImage(ctx, rect, cgImage);
  
    CGImageRelease(cgImage);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
  }
}

@end

@interface WindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) vwindow *window;
@end

@implementation WindowDelegate
- (void)windowDidResize:(NSNotification *)notification {
  NSWindow *window = notification.object;
  NSRect frame = [window contentView].frame;

  self.window->width = (u32)frame.size.width;
  self.window->height = (u32)frame.size.height;

  if (state_ptr && state_ptr->window_resize_callback) {
    state_ptr->window_resize_callback(self.window);
  }
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
  if (state_ptr && state_ptr->window_close_callback) {
    state_ptr->window_close_callback(self.window);
  }

  return YES;
}
@end

b8 platform_initialize() {
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
    
    state_ptr->windows = vec_create(vwindow *, 1);
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
  }
  return true;
}

void platform_uninitalize(void)
{
    if (state_ptr) {
        u32 len = vec_length(state_ptr->windows);
        for (u32 i = 0; i < len; ++i) {
            if (state_ptr->windows[i] != PNULL) {
                // gota free the window some how
                //DestroyWindow(state_ptr->windows[i]->platform_state->window);
                state_ptr->windows[i]->platform_state->window = PNULL;
                free(state_ptr->windows[i]->platform_state);
                state_ptr->windows[i]->platform_state = PNULL;
                state_ptr->windows[i] = NULL;
                //[0][NULL][2][3][4]...
                //   ^--freed window
                // In platform_window_create I call darray_push(state_ptr->windows, out_window);
                // this could break because old indexes could not be resued, though it breaks from many creates and destroys.
            }
        }

        vec_destroy(state_ptr->windows);
        state_ptr->windows = PNULL;
        state_ptr->window_resize_callback = PNULL;
        state_ptr->window_close_callback = PNULL;
        free(state_ptr);
        state_ptr = PNULL;
    }
}

b8 platform_window_create(platform_string title, u32 width, u32 height, u32 x, u32 y, vwindow* window) {

  if (!state_ptr) {
    return false;
  }

  vwindow_platform_state *state = malloc(sizeof(vwindow_platform_state));
  if (!state) {
    return false;
  }
  @autoreleasepool {
  state->window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(x, y, width, height)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  if (!state->window) {
    free(state);
    return false;
  }

  if (!title) {
    //[state->window setTitle:[NSString stringWithUTF8String:"macos window"]];
    [state->window setTitle:@("macos window")];
  }

  state->delegate = [[WindowDelegate alloc] init];
  if (!state->delegate) {
    free(state);
    return false;
  }

  state->delegate.window = window;
  [state->window setDelegate:state->delegate];

  [state->window makeKeyAndOrderFront:nil];
  window->platform_state = state;
  
  // get the size of the client area
  NSRect contentBounds = [state->window.contentView bounds];

  state->view = [[CanvasView alloc] initWithFrame:contentBounds];
  if(!state->view) {
    return false;
  }
  state->window.contentView = state->view;
  state->view.window = window;

  window->width = (u32)contentBounds.size.width;
  window->height = (u32)contentBounds.size.height;
  vec_push(state_ptr->windows, window);
  }
  return true;
}

void platform_window_destroy(vwindow *window) {
  if (!window || !window->platform_state)
    return;
    
  @autoreleasepool {
    u32 const len = vec_length(state_ptr->windows);
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
        // In platform_window_create I call vec_push(state_ptr->windows, out_window);
        return;
      }
    }
  }
}

static vwindow *vwindow_from_NSWindow(NSWindow *window,
                                                  u64 *out_index) {
  for (u64 i = 0; i < vec_length(state_ptr->windows); i++) {
    if (state_ptr->windows[i]->platform_state->window == window) {
      if (out_index) {
        *out_index = i;
      }
      return state_ptr->windows[i];
    }
  }
  return 0;
}


// FIX THIS. I can't seem to get this to work exactly the way I want it to.
// This is fine for now. I might just set the layer.contents, which would give me
// more high-level control over what is rendered. The con is that I would
// have to manage my own framebuffer and ensure it is always the same size
// as the view, which should be about the same size as the window,
// directly avoiding the callback.

// also just like windowsL // Unlike XCB, my window remains the same color as the [[NSColor blackColor] setFill]; when the image size is too big.
// When I scale the image window and it becomes smaller than the image, the screen goes completely black.
// When scaling in and out, the image flickers between showing the image and rendering black.
// But it works. WILL COME BACK TO THIS ON THE NEXT COMMIT.

void platform_window_present_frame(
    vwindow *window,
    u8 *pixel_map,
    u32 width,
    u32 height,
    u32 x,
    u32 y)
{
    vwindow_platform_state *state = window->platform_state;

    state->pixel_map = pixel_map;
    state->pixel_width = width;
    state->pixel_height = height;
    state->pixel_x = x;
    state->pixel_y = y;

    [state->view setNeedsDisplay:YES];
}

void platform_pump_messages() {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    NSEvent *event = PNULL;
    do {
      event = [app nextEventMatchingMask:NSEventMaskAny
                               untilDate:nil
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:TRUE];

      [app sendEvent:event];
    } while (event != PNULL);
  }
}

void platform_write_console(CONSOLE_SINK sink, platform_string message){
    platform_string_internal imessage = message;
    FILE* stream = (sink == CONSOLE_SINK_OUT)? stdout : stderr;
    fprintf(stream, "%s", imessage);
}
// Cache the page size to eliminate sysconf overhead
u64 platform_page_size(void) {
    static u64 page_size = 0;
    if (page_size == 0) {
        long sz = sysconf(_SC_PAGESIZE);
        page_size = (sz > 0) ? (u64)sz : 4096;
    }
    return page_size;
}

u64 platform_page_align(u64 size) {
    u64 page_size = platform_page_size();
    return (size + page_size - 1) & ~(page_size - 1);
}

b8 platform_virtual_reserve(vvirtual_memory* virtual, u64 to_reserve)
{
    if (!virtual || to_reserve == 0) return false;

    u64 aligned_reserve = platform_page_align(to_reserve);

    void* map = mmap(NULL, aligned_reserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        return false;
    }

    virtual->base = map;
    virtual->committed = 0;
    virtual->reserved = aligned_reserve;
    return true;
}

void* platform_virtual_commit(vvirtual_memory* virtual, u64 to_commit)
{
    if (!virtual || !virtual->base || to_commit == 0) return PNULL;

    u64 aligned_commit = platform_page_align(to_commit);

    // Bounds check using the ALIGNED commit size
    if (virtual->committed + aligned_commit > virtual->reserved) return PNULL;

    uint8_t* commit_addr = (uint8_t*)virtual->base + virtual->committed;
    
    if (mprotect(commit_addr, aligned_commit, PROT_READ | PROT_WRITE) != 0) {
        return PNULL;
    }

    virtual->committed += aligned_commit;
    return commit_addr;
}

void platform_virtual_decommit(vvirtual_memory* virtual, u64 amount)
{
    if (!virtual || !virtual->base || amount == 0 || amount > virtual->committed) return;

    // Align decommit amount up to whole page boundaries
    u64 aligned_amount = platform_page_align(amount);
    if (aligned_amount > virtual->committed) {
        aligned_amount = virtual->committed;
    }

    u64 new_committed = virtual->committed - aligned_amount;
    uint8_t* decommit_addr = (uint8_t*)virtual->base + new_committed;

    // Revoke permissions on full page boundaries
    mprotect(decommit_addr, aligned_amount, PROT_NONE);

    // Return physical pages back to OS
#ifdef MADV_DONTNEED
    madvise(decommit_addr, aligned_amount, MADV_DONTNEED);
#endif

    virtual->committed = new_committed;
}

void platform_virtual_unreserve(vvirtual_memory* virtual)
{
    if (!virtual || !virtual->base) return;

    munmap(virtual->base, virtual->reserved);
    
    virtual->base = PNULL;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void* platform_heap_allocate(u64 to_alloc){
    return malloc(to_alloc);
}

void platform_heap_deallocate(void* memory){
    free(memory);
}

void* platform_zero_memory(void* memory, u64 memory_size){
    return memset(memory, 0, memory_size);
}

void* platform_set_memory(void* memory, i32 value, u64 memory_size){
    return memset(memory, value, memory_size);
}

void* platform_copy_memory(void* dest, void* src, u64 memory_size){
    return memcpy(dest, src, memory_size);
}


void platform_set_window_resize_callback(
    platform_window_resize_callback callback) {
  state_ptr->window_resize_callback = callback;
}

void platform_set_window_close_callback(
    platform_window_close_callback callback) {
  state_ptr->window_close_callback = callback;
}

u32 platform_string_to_utf8(platform_string plf_str, char* utf8_str, u32 utf8_str_len)
{
    if(utf8_str_len){
        memcpy(utf8_str, plf_str, utf8_str_len);
        return utf8_str_len;
    }

    if(!plf_str) return 0;
    return strlen(plf_str);
}

u32 utf8_to_platform_string(char* utf8_str, platform_string plf_str, u32 plf_str_len)
{
    if(plf_str_len){
        memcpy(plf_str, utf8_str, plf_str_len);
        return plf_str_len;
    }
    if(!utf8_str) return 0; 
    return strlen(utf8_str);
}
#endif