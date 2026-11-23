#include "core/defines.h"
#include "core/logger.h"
#include "core/memory/memory.h"
#include "core/platform/platform.h"
#include "image/image_util.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) vallocate(sz)
#define STBI_REALLOC(p, newsz) vreallocate(p, newsz)
#define STBI_FREE(p) vdeallocate(p)
#include "image/stb_image.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_POSITION_X 0
#define WINDOW_POSITION_Y 0
#define WINDOW_TITLE "Image Viewer"

#define CLEAR_COLOR_WHITE 255
typedef struct application_state {
    vwindow window;

    Image original_buffer;

    Image render_target;
    vvirtual_memory render_target_buffer_virtual;

    b8 is_running;
} application_state;

application_state app_state;

void on_window_resize(vwindow *window) {
    u64 const needed = (u64)window->width * window->height * sizeof(u32);
    if (needed > app_state.render_target_buffer_virtual.committed) {
        platform_virtual_commit(
            &app_state.render_target_buffer_virtual,
            needed - app_state.render_target_buffer_virtual.committed);
    }
    app_state.render_target.width = window->width;
    app_state.render_target.height = window->height;

    clear_color(&app_state.render_target, CLEAR_COLOR_WHITE);
    const i32 ASPECT_H = app_state.original_buffer.height;
    const i32 ASPECT_W = app_state.original_buffer.width;
    i32 dst_w = window->width;
    i32 dst_h = (window->width * ASPECT_H) / ASPECT_W;

    if (dst_h > window->height) {
        dst_h = window->height;
        dst_w = (window->height * ASPECT_W) / ASPECT_H;
    }

    i32 const pos_x = (window->width - dst_w) / 2;
    i32 const pos_y = (window->height - dst_h) / 2;

    Image image;
    image.width = dst_w;
    image.height = dst_h;

    blit_nearest(&app_state.render_target, &app_state.original_buffer, pos_x,
                 pos_y, image.width, image.height);

    platform_window_present_frame(window, (u8 *)app_state.render_target.pixels);
}
void on_window_close(vwindow *window) {
    VDEBUG("window closed.\n");
    app_state.is_running = false;
}

b8 application_create(application_state *application) {
    command_line_args args = platform_get_command_line_args();
    if (args.argc < 2) {
        VERROR("usage prog <image-path>\n");
        return false;
    }

    int image_channels = 4;
    app_state.original_buffer.pixels = (void *)stbi_load(
        args.argv[1], &app_state.original_buffer.width,
        &app_state.original_buffer.height, &image_channels, 4);

    if (!app_state.original_buffer.pixels) {
        VERROR("Failed to load the image named %s\n", args.argv[1]);
        return false;
    }

    VDEBUG("image named %s loaded with a width of %i and a height of %i\n",
           args.argv[1], app_state.original_buffer.width,
           app_state.original_buffer.height);

    swizzle_rgba_to_bgra_horizontal(&app_state.original_buffer);

    app_state.render_target.width = WINDOW_WIDTH;
    app_state.render_target.height = WINDOW_HEIGHT;

    // RESERVE MY ABSURD AMOUNT OF MEMORY THAT I MAY NEVER USE ALL
    // BUT IT IS VIRUTLE and NOT physical, so all good
    platform_virtual_reserve(&application->render_target_buffer_virtual, GB(1));
    application->render_target.pixels = platform_virtual_commit(
        &application->render_target_buffer_virtual, MB(30));
    if (!application->render_target.pixels) {
        VERROR(
            "Failed to commit image renderring buffer to reserved memory.\n");
        stbi_image_free(app_state.original_buffer.pixels);
        app_state.original_buffer.height = 0;
        app_state.original_buffer.width = 0;
        return false;
    }

    platform_set_window_resize_callback(on_window_resize);
    platform_set_window_close_callback(on_window_close);
    vwindow_config window_config;
    window_config.title = WINDOW_TITLE;
    window_config.width = WINDOW_WIDTH;
    window_config.height = WINDOW_HEIGHT;
    window_config.xposition = WINDOW_POSITION_X;
    window_config.yposition = WINDOW_POSITION_Y;

    if (!platform_window_create(&window_config, &application->window)) {
        VERROR("Failed to Create Window\n");
        platform_virtual_unreserve(&app_state.render_target_buffer_virtual);
        stbi_image_free(app_state.original_buffer.pixels);
        app_state.original_buffer.height = 0;
        app_state.original_buffer.width = 0;
        return false;
    };
    VDEBUG("window created.\n");
    application->is_running = true;
    on_window_resize(&app_state.window);
    return true;
}

void application_update(application_state *application) {
    while (application->is_running) {
        platform_process_events();
    }
}

void application_destroy(application_state *application) {
    platform_virtual_unreserve(&application->render_target_buffer_virtual);
    platform_window_destroy(&application->window);
    stbi_image_free(application->original_buffer.pixels);
    VDEBUG("application destroyed.\n");
}

int application_main(void) {
    u64 total_memory_requirment = 0;
    u64 memory_requirment;

    memory_requirment = 0;
    platform_initialize(&memory_requirment, nullptr, nullptr);

    total_memory_requirment += memory_requirment;
    void *memory = vallocate(total_memory_requirment);
    if (!memory) {
        VFATAL("Failed allocate needed memory for the application.\n");
    }

    struct platform_state *_platform_state = memory;
    if (!platform_initialize(&memory_requirment, memory, _platform_state)) {
        VFATAL("Failed to initialize platform\n");
        return EXIT_FAILURE;
    }

    if (application_create(&app_state)) {
        application_update(&app_state);
        application_destroy(&app_state);
    }

    platform_uninitialize();
    vdeallocate(memory);
    return EXIT_SUCCESS;
}
