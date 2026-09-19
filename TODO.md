# Rendering Architecture

The goal of the platform layer:

Window creation, destruction, a few callbacks, and framebuffer rendering using the native API, independent of OpenGL or Vulkan.

I will only support framebuffer rendering, not other types of rendering. Because drawing a framebuffer is really the only function we need. We don't need GLFW, SDL, or direct OpenGL use for rendering shapes. What we can do is draw on the framebuffer by having our own high-level functions like `draw_line`, `draw_point`, `draw_image_rect`, etc., and when done, call something like `platform_window_present_frame`.

Ideal: allocate a memory buffer (which the user space owns), draw to it using platform-independent functions, and call that one API function to render a frame. This is basic CPU rendering using only the native API.

Remember: while I can abstract away platform-specific graphics handles such as `HDC`, `xcb_gcontext_t`, and `CGContextRef`, they of course do not function the same way.

On Win32, I can obtain an `HDC` from `BeginPaint()` or `GetDC()`. Do I really need to keep track of the `HDC` returned by `BeginPaint()` just to call `platform_graphics_context_put_image()`?

`XCB_EXPOSE` does not map cleanly to `WM_PAINT` unless I call `UpdateWindow` right after window creation. However, it is crashing because the resources are not initialized.

Consider replacing the current window-render callbacks with per-frame rendering:

**Event-based rendering:** potentially more efficient, but introduces platform-specific behavior and inconsistent semantics.

**Per-frame rendering:** potentially does unnecessary work, but provides a much simpler and more consistent engine architecture.

## How should the framebuffer be rendered?

### Intermediate mode

Lets the engine choose where the renderer's framebuffer is allocated and gives it greater control over the framebuffer's memory, layout, lifetime, and potentially synchronization.

### Direct mode

Lets the graphics API/windowing system manage the presentation framebuffer internally. The renderer submits or presents directly to that framebuffer, potentially avoiding an extra copy and therefore potentially being faster. However, the engine has less control over how the framebuffer is allocated and managed.

## XCB image presentation

`xcb_put_image` vs. `xcb_shm_put_image`

`darray` has holes in it when I destroy the window.

If UTF is different for Windows vs. Linux and macOS, how about during debug time having macOS and Linux allocate `string_length * 2` to ensure the program has enough memory to begin with, and in release setting it back to `string_length * 1`? This would be a guard to ensure the code also works on Windows.

One thing that is off is that Windows needs a memory allocation every time the application communicates with the platform layer for string operations. That creates inconsistency in how much memory should be allocated for the application. Maybe I am wrong, but the allocations for an app should be almost constant across platforms.

Maybe a scratch arena or thread-local storage for the window's strings.

## Event-Driven Rendering
Rendering pieces of the image to a window instead of the whole image, assuming the window client area is the same size as the framebuffer.

User input seems like a platform-specific thing, but a flexible operation where the user tells the platform the size and position of the image.

Rendering seems more platform-specific because Windows and macOS don't seem to have good semantics for rendering on demand. It requires me to store data that I don't want to store, like an HDC or the entire information about what I am rendering, until the platform-specific callbacks feel like they should render, such as drawRect and WM_PAINT( though windows is slighlty more flexable than mac)


When the user does something or per-frame rendering.

May want to consider `GetMessageW` over `PeekMessageW`.

`xcb_wait_for_event` over `xcb_poll_for_event`.

## MOST OF ALL: FIX ALLOCATION

Because allocation is inconsistent.

```text
malloc
platform_allocate
virtual_reserve / virtual_commit
```
COme back to logger. I feel logging and any output system should not have to rely on allocations to work. Its one and only job should be logging selected things such as `Win32_DebugString`, `platform_console`, or `output.txt`.
