## XCB image presentation

`xcb_put_image` vs. `xcb_shm_put_image`

# Event/Message Loop
May want to consider `GetMessageW` over `PeekMessageW`.

`xcb_wait_for_event` over `xcb_poll_for_event`.

## MOST OF ALL: FIX ALLOCATION

Because allocation is inconsistent.

```text
malloc 
platform_allocate
virtual_reserve / virtual_commit
```
Come back to logger. I feel logging should atlease have fallback when all fail, espacilly when memory allocation fails. This must mean the the must reliable functions must always be the logger and therefore platform_write_console and asserting. If this basic funcally are not working there are bigger problems that must be adressed first. 

Possily have option for adding multiple sinks to output to multiple sources.
`Win32_DebugString`, `platform_console`, or `output.txt`.

`darray/vec` has holes in it when I destroy the window.

macOS seems to segfault unexpectedly, but not frequently enough for me to predictably reproduce it or determine where the error is. After leaving the app running for over 7 minutes and then closing it, I think that’s when it segfaulted. I’ll have to test this more later.

In debug, the last printed message was:

[DEBUG]: window closed.

Seems to be fixed, will leave this here for me to keep checking for it.
zsh: segmentation fault ./prog ../examples/T.jpeg
what seems to have casused the error was forgetting a & in 
vec_push(state_ptr->windows, <here>window);

Really stuck on what to do with WriteConsoleW (UTF8 to UTF16).
Each with there own trade offs
-thread local buffer
-stack allocation
-heap allocated
-static stack buffer that is thread safe

Come back to:
vstring.h vstring.c
memory.h memory.c

## Things that should be prioty always:
1. Some type of way to log a message or assert
2. Memory allocation
