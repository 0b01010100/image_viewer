# Rendering Architecture

The goal of the platform layer:

Allocate a memory buffer, draw to it using platform independent functions, and call that one API function to render a frame to the whole window. This is basic CPU rendering using framebuffer and only the native API to present.

Window creation, destruction, a few callbacks, and framebuffer rendering using the native API, independent of OpenGL or Vulkan.

Pretty much an experiment with using framebuffers for rendering without the used of GLFW, SDL, or direct OpenGL as of now. What we can do is draw on the framebuffer by having our own high-level functions like `draw_line`, `draw_point`, `draw_image_rect`, etc., and when done, call something like `platform_window_present_frame`.

## XCB image presentation

`xcb_put_image` vs. `xcb_shm_put_image`

`darray/vec` has holes in it when I destroy the window.

If UTF is different for Windows vs. Linux and macOS, how about during debug time having macOS and Linux allocate `string_length * 2` to ensure the program has enough memory to begin with, and in release setting it back to `string_length * 1`? This would be a guard to ensure the code also works on Windows.

One thing that is off is that Windows needs a memory allocation every time the application communicates with the platform layer for string operations. That creates inconsistency in how much memory should be allocated for the application. Maybe I am wrong, but the allocations for an app should be almost constant across platforms.

Maybe a scratch arena or thread-local storage for the window's strings.

May want to consider `GetMessageW` over `PeekMessageW`.

`xcb_wait_for_event` over `xcb_poll_for_event`.

## MOST OF ALL: FIX ALLOCATION

Because allocation is inconsistent.

```text
malloc 
platform_allocate
virtual_reserve / virtual_commit
```
Come back to logger. I feel logging and any output system should not have to rely on allocations to work. Its one and only job should be logging selected things such as `Win32_DebugString`, `platform_console`, or `output.txt`.

As of now there are 3 big allocations happing and I think I can get ride of one:
    - Big allocation for framebuffer: memory allocation = dynaiclly scales when the window extensed to a new size.
    - Orginal Image: memory allocation type = static
    - The main Image: memory allocation type = static for image itself stays the same.

    This plan is to not not have a main Image buffer and sample the original image onto the presetn buffer directly instead of sample to main image and them copy to spefcail cordante of the framebuffer. gotta learn MORE math i don't know to make this work.

It is too much work to handle a generic `platform_string`. I think it would be better to define something specifically for each window platform to handle its UTF wide-character strings.

For example:

```c
// Use the stack for strings
#define WIN32_USE_STACK_FOR_STRINGS
#define WIN32_MAX_STRING_CONVERT_STACK 1024

#if !defined(WIN32_MAX_STRING_CONVERT_STACK)

#endif

// Use scratch allocator for strings
#define WIN32_USE_SCRATCH_FOR_STRINGS
```

These can be passed as compile-time constants:

```text
-DWIN32_USE_STACK_FOR_STRINGS -DWIN32_MAX_STRING_CONVERT_STACK=1024
```

This way, the code does not have to be manually changed if the stack frame for some functions needs to be larger or smaller.

For example, instead of having a generic `platform_string`, the Windows platform layer can handle the UTF-8 to UTF-16 conversion internally using either the stack or a scratch allocator.

For macOS, I need to look more into `NSString`. What I think is happening is that macOS is not necessarily using UTF-8 everywhere, but rather the APIs it uses, such as Cocoa/Foundation, work with UTF-16-style Unicode strings.

macos seems to segfault unexaptedly but not enoguth for me to measure pridecitable to know where the error. after leving the app for over 7 mintues and then clsoing i think that when it segfaulted. will have to test this later. in debug the last printed message was "[DEBUG]: window closed.
zsh: segmentation fault  ./prog ../examples/T.jpeg"