// CODE FOR MACOS
#include "platform.h"

#ifdef PLATFORM_MACOS
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include "../containers/vec.h"
#include "../logger.h"
#include "../memory/memory.h"

@class ApplicationDelegate;
@class CanvasView;
@class WindowDelegate;

// may rename to application info or something
// should have information that is know during startup time
typedef struct platform_command_line_args {
    int argc;
    char **argv;
} platform_command_line_args;

platform_command_line_args application_args;
platform_state *state_ptr;
vwindow *vwindow_from_NSWindow(NSWindow *Window);

typedef struct macos_handle_info {
    void *unused;
} macos_handle_info;

struct platform_state {
    macos_handle_info handle;
    ApplicationDelegate *app_delegate;
    vwindow **windows; // vec
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
};

struct vwindow_platform_state {
    NSWindow *Window;
    WindowDelegate *WindowDelegate;
    CanvasView *View;
};

@interface ApplicationDelegate : NSObject <NSApplicationDelegate> {
}
@end // ApplicationDelegate

@implementation ApplicationDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Posting an empty event at start
    @autoreleasepool {
        NSEvent *event =
            [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                               location:NSMakePoint(0, 0)
                          modifierFlags:0
                              timestamp:0
                           windowNumber:0
                                context:nil
                                subtype:0
                                  data1:0
                                  data2:0];
        [NSApp postEvent:event atStart:YES];
    } // autoreleasepool

    [NSApp stop:nil];
}
@end // ApplicationDelegate

@interface CanvasView : NSView {
    vwindow *handle;
}
@end

@implementation CanvasView

- (instancetype)initWithWindow:(vwindow *)wnd {
    self = [super initWithFrame:NSMakeRect(0, 0, wnd->width, wnd->height)];
    if (self) {
        handle = wnd;
        self.wantsLayer = YES;
        // self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // move origin to top left
        self.layer.contentsGravity = kCAGravityTopLeft;

        // Make sure the layer actually has a drawable size.
        self.layer.contentsGravity = kCAGravityResize;
    }
    return self;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}
@end // CanvasView

@interface WindowDelegate : NSObject <NSWindowDelegate> {
    vwindow *window;
}
- (instancetype)initWithState:(vwindow *)window_state;
@end // WindowDelegate

@implementation WindowDelegate
- (instancetype)initWithState:(vwindow *)init_state {
    self = [super init];

    if (self != nil) {
        window = init_state;
    }
    return self;
}
- (void)windowDidResize:(NSNotification *)notification {
    NSRect frame = [window->platform_state->Window contentView].frame;

    window->width = (i32)frame.size.width;
    window->height = (i32)frame.size.height;

    if (state_ptr && state_ptr->window_resize_callback) {
        state_ptr->window_resize_callback(window);
    }
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    if (state_ptr && state_ptr->window_close_callback) {
        state_ptr->window_close_callback(window);
    }

    return YES;
}
@end

b8 platform_initialize(u64 *memory_requirement, void *memory,
                       struct platform_state *state) {
    UNUSED(vwindow_from_NSWindow);
    if (state_ptr) {
        return false;
    }
    if (!state) {
        *memory_requirement = sizeof(platform_state);
        return true;
    }

    if (!memory) {
        VERROR("memory can't be null");
        return false;
    }

    state_ptr = state;

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];

        state_ptr->windows = vec_create(vwindow *, 1);
        state_ptr->window_resize_callback = nullptr;
        state_ptr->window_close_callback = nullptr;

        @autoreleasepool {
            [NSApplication sharedApplication];
            state_ptr->app_delegate = [[ApplicationDelegate alloc] init];
            if (!state_ptr->app_delegate) {
                VERROR("Failed to create application");
            }
            [NSApp setDelegate:state_ptr->app_delegate];

            if (![[NSRunningApplication currentApplication]
                    isFinishedLaunching]) {
                [NSApp run];
            }

            [NSApp activateIgnoringOtherApps:YES];
        }
        return true;
    }
}
void platform_uninitialize(void) {
    if (!state_ptr) {
        VFATAL("Double free corruption");
        return;
    }
    @autoreleasepool {
        u32 len = vec_length(state_ptr->windows);
        for (u32 i = 0; i < len; ++i) {
            if (state_ptr->windows[i] != nullptr &&
                state_ptr->windows[i]->platform_state != nullptr) {
                // destroy window
                state_ptr->windows[i]->platform_state->WindowDelegate = nil;
                if (state_ptr->windows[i]->platform_state->Window) {
                    state_ptr->windows[i]->platform_state->Window.contentView =
                        nil;

                    state_ptr->windows[i]->platform_state->View = nil;

                    [state_ptr->windows[i]->platform_state->Window
                        setDelegate:nil];
                    [state_ptr->windows[i]->platform_state->Window close];
                    state_ptr->windows[i]->platform_state->Window = nil;
                }

                vdeallocate(state_ptr->windows[i]->platform_state);
                state_ptr->windows[i]->platform_state = nullptr;
                state_ptr->windows[i] = nullptr;
                return;
            }
        }

        vec_destroy(state_ptr->windows);
    }
}

command_line_args platform_get_command_line_args(void) {
    command_line_args args;
    args.argc = application_args.argc;
    args.argv = application_args.argv;
    return args;
}

b8 platform_window_create(const vwindow_config* config,
                          vwindow *window) {
    @autoreleasepool {
        (void)window->width;
        (void)window->height;

        vwindow_platform_state *window_state =
            vallocate(sizeof(vwindow_platform_state));
        if (!window_state) {
            return false;
        }
        char* title = config->title;
        if (!title) {
            title = "appkit_window";
        }

        window_state->Window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(config->xposition, config->yposition, config->width, config->height)
                      styleMask:NSWindowStyleMaskTitled |
                                NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable |
                                NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered
                          defer:NO];

        if (window_state->Window == nil) {
            vdeallocate(window_state);
            return false;
        }
        window_state->View = [[CanvasView alloc] initWithWindow:window];
        if (!window_state->View) {
            window_state->Window = nil;
            vdeallocate(window_state);
            return false;
        }
        window_state->WindowDelegate =
            [[WindowDelegate alloc] initWithState:window];
        if (!window_state) {
            window_state->View = nil;
            window_state->Window = nil;
            vdeallocate(window_state);
            return false;
        }
        [window_state->Window setLevel:NSNormalWindowLevel];
        [window_state->Window setContentView:window_state->View];
        [window_state->Window setBackgroundColor:NSColor.blackColor];
        [window_state->Window setTitle:@(title)];
        [window_state->Window setIsVisible:YES];
        [window_state->Window makeKeyAndOrderFront:nil];
        [window_state->Window setDelegate:window_state->WindowDelegate];
        window->platform_state = window_state;
        window->renderer_state = nullptr;

        window->width = config->width;
        window->height = config->height;
        vec_push(state_ptr->windows, &window);
    }
    return true;
}

void platform_window_destroy(vwindow *window) {
    if (!window || !window->platform_state) {
        return;
    }

    @autoreleasepool {
        u32 len = vec_length(state_ptr->windows);
        for (u32 i = 0; i < len; ++i) {
            if (state_ptr->windows[i] == window) {
                [window->platform_state->Window setDelegate:nil];
                [window->platform_state->Window close];

                window->platform_state->WindowDelegate = nil;

                window->platform_state->Window.contentView = nil;
                window->platform_state->Window = nil;

                window->platform_state->Window = nil;
                window->platform_state->View = nil;
                //[0][nullptr][2][3][4]...
                //   ^--freed window
                // In platform_window_create I call
                // darray_push(state_ptr->windows, out_handle); this could break
                // because old indexes could not be resued, though it breaks
                // from many creates and destroys.
                vdeallocate(window->platform_state);
                state_ptr->windows[i] = nullptr;
                window->platform_state = nullptr;
                return;
            }
        }
        VERROR("Destroying a window that was somehow not registered with the "
               "platform layer.\n");
        [window->platform_state->Window close];

        window->platform_state->WindowDelegate = nil;

        window->platform_state->Window.contentView = nil;
        window->platform_state->Window = nil;

        window->platform_state->Window = nil;
        window->platform_state->View = nil;
    }
}

vwindow *vwindow_from_NSWindow(NSWindow *Window) {
    for (u64 i = 0; i < vec_length(state_ptr->windows); i++) {
        if (!state_ptr->windows[i]) {
            continue;
        }
        if (state_ptr->windows[i]->platform_state->Window == Window) {
            return state_ptr->windows[i];
        }
    }
    return 0;
}

void platform_window_present_frame(vwindow *window, u8 *pixel_map) {
    if (!window || !window->platform_state || !pixel_map) {
        return;
    }
    vwindow_platform_state *window_state = window->platform_state;

    size_t width = (size_t)window->width;
    size_t height = (size_t)window->height;
    size_t bytes_per_row = width * sizeof(u32);
    size_t data_size = bytes_per_row * height;

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    CGDataProviderRef provider =
        CGDataProviderCreateWithData(nil, pixel_map, data_size, nullptr);

    CGImageRef image = CGImageCreate(
        width, height, 8, 32, bytes_per_row, colorSpace,
        kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider,
        nullptr, false, kCGRenderingIntentDefault);

    window_state->View.layer.contents = (__bridge id)image;

    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
}

void platform_process_events(void) {
    if (state_ptr) {
        @autoreleasepool {
            NSEvent *event;

            for (;;) {
                event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
                if (!event) {
                    break;
                }
                [NSApp sendEvent:event];
            }
        }
    }
}

void platform_write_console(CONSOLE_SINK sink, const char *message) {
    FILE *stream = (sink == CONSOLE_SINK_OUT) ? stdout : stderr;
    fprintf(stream, "%s", message);
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

b8 platform_virtual_reserve(vvirtual_memory *virtual, u64 to_reserve) {
    if (!virtual || to_reserve == 0) {
        return false;
    }
    u64 aligned_reserve = platform_page_align(to_reserve);

    void *map = mmap(nullptr, aligned_reserve, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        return false;
    }

    virtual->base = map;
    virtual->committed = 0;
    virtual->reserved = aligned_reserve;
    return true;
}

void *platform_virtual_commit(vvirtual_memory *virtual, u64 to_commit) {
    if (!virtual || !virtual->base || to_commit == 0) {
        return nullptr;
    }
    u64 aligned_commit = platform_page_align(to_commit);

    // Bounds check using the ALIGNED commit size
    if (virtual->committed + aligned_commit > virtual->reserved) {
        return nullptr;
    }
    u8 *commit_addr = (u8 *)virtual->base + virtual->committed;

    if (mprotect(commit_addr, aligned_commit, PROT_READ | PROT_WRITE) != 0) {
        return nullptr;
    }

    virtual->committed += aligned_commit;
    return commit_addr;
}

void platform_virtual_decommit(vvirtual_memory *virtual, u64 amount) {
    if (!virtual || !virtual->base || amount == 0 ||
        amount > virtual->committed) {
        return;
    }
    // Align decommit amount up to whole page boundaries
    u64 aligned_amount = platform_page_align(amount);
    if (aligned_amount > virtual->committed) {
        aligned_amount = virtual->committed;
    }

    u64 new_committed = virtual->committed - aligned_amount;
    u8 *decommit_addr = (u8 *)virtual->base + new_committed;

    // Revoke permissions on full page boundaries
    mprotect(decommit_addr, aligned_amount, PROT_NONE);

    // Return physical pages back to OS
#ifdef MADV_DONTNEED
    madvise(decommit_addr, aligned_amount, MADV_DONTNEED);
#endif

    virtual->committed = new_committed;
}

void platform_virtual_unreserve(vvirtual_memory *virtual) {
    if (!virtual || !virtual->base) {
        return;
    }
    munmap(virtual->base, virtual->reserved);

    virtual->base = nullptr;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void *platform_heap_allocate(u64 to_alloc) {
    return malloc(to_alloc);
}

void platform_heap_deallocate(void *memory) {
    free(memory);
}

void *platform_zero_memory(void *memory, u64 memory_size) {
    return memset(memory, 0, memory_size);
}

void *platform_set_memory(void *memory, i32 value, u64 memory_size) {
    return memset(memory, value, memory_size);
}

void *platform_copy_memory(void *dest, const void *src, u64 memory_size) {
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

// may use a macro for this to allow name change via compiler flag
extern int application_main(void);

// int platform_main(int argc, char **argv)
int main(int argc, char **argv) {
    application_args.argc = argc;
    application_args.argv = argv;
    return application_main();
}
#endif
