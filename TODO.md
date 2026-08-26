Rendering architecture
the goal of the platform layer:
window creation, destroy, few callbacks + frame_buffer renderering using native api independant from OpenGL or Vulkan.

I will only suypport frame buffer rendering not other types of renderering. becasue drawing a frame buffer is really the only function we need. We don't need GLFW, SDL, or direct OpenGL use for renderering shapes. What we can do is draw on the frame buffer by having our own high level functions like draw_line, draw_point_ draw_image_rect, .etc and when done call something like platform_window_present_frame. Ideal: allocate memoy buffer(which user space owns) + draw it is using platform indpendatn functions + call that one api function to render a frame. This is basic CPU rendering only using the native api.

Remember: while I can abstract away platform-specific graphics handles such as HDC, xcb_gcontext_t, and CGContextRef, they do not function the same way. The abstraction should hide their platform-specific details without pretending that they have identical semantics.

On Win32, I can obtain an HDC from BeginPaint() or GetDC(). Do I really need to keep track of the HDC returned by BeginPaint() just to call platform_graphics_context_put_image()?
XCB_EXPOSE does not map cleanly to WM_PAINT unless I call UpdateWindow. This makes event-based rendering awkward as a cross-platform abstraction.
Consider replacing the current window-render callbacks with per-frame rendering:
Event-based rendering: potentially more efficient, but introduces platform-specific behavior and inconsistent semantics.
Per-frame rendering: potentially does unnecessary work, but provides a much simpler and more consistent engine architecture.
Event-based rendering is pushing platform-specific behavior into the higher-level renderer. For example:
XCB_EXPOSE can occur immediately after window creation.
WM_PAINT does not necessarily behave the same way; depending on how the window is invalidated, the initial image may not be rendered through the same path until a resize or another invalidation occurs.
Therefore, treating platform paint events as a universal "render now" event is unreliable.
How should the framebuffer be rendered?

Intermediate mode

Lets the engine choose where the renderer's framebuffer is allocated and gives it greater control over the framebuffer's memory, layout, lifetime, and potentially synchronization.

Direct mode

Lets the graphics API/windowing system manage the presentation framebuffer internally. The renderer submits or presents directly to that framebuffer, potentially avoiding an extra copy and therefore potentially being faster. However, the engine has less control over how the framebuffer is allocated and managed.

XCB image presentation
xcb_put_image
Sends the image data through the normal X11 request mechanism.
Simple and broadly applicable.
The pixel data is transferred to the X server as part of the request.
xcb_shm_put_image
Uses the X11 MIT-SHM extension.
The client and X server access an X11 shared-memory segment containing the image data.
Can avoid the normal transfer of the entire pixel buffer through the X11 protocol and can therefore be significantly more efficient for large or frequently updated images.
