#include "application.h"
#include "core/defines.h"
#include "core/platform/platform.h"
#define STB_IMAGE_IMPLEMENTATION
#include "image/stb_image.h"
#include "image/image_util.h"
#include "core/logger.h"
#include "core/allocators/arena.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_TITLE "Image Viewer"

typedef struct application_state {
    vwindow window;

    Image original_buffer;

    Image front_buffer;
    vvirtual_memory front_buffer_virtual;

    // may delete this
    Image main_image;
    vvirtual_memory main_image_virtual;
    
    b8 is_running;
}application_state;

application_state app_state;

void on_window_resize(vwindow* window)
{
    u64 needed = (u64)window->width * window->height * sizeof(u32);
    if (needed > app_state.front_buffer_virtual.committed) {
        platform_virtual_commit(
            &app_state.front_buffer_virtual,
            needed - app_state.front_buffer_virtual.committed
        );
    }
    app_state.front_buffer.width = window->width;
    app_state.front_buffer.height = window->height;

    clear_color(&app_state.front_buffer, 255); 
    const u32 ASPECT_H = app_state.original_buffer.height;
    const u32 ASPECT_W = app_state.original_buffer.width;
    
    u32 dst_w = window->width;
    u32 dst_h = (window->width * ASPECT_H) / ASPECT_W;
    
    if (dst_h > window->height) {
        dst_h = window->height;
        dst_w = (window->height * ASPECT_W) / ASPECT_H;
    }
    
    i32 pos_x = (i32)(window->width - dst_w) / 2;
    i32 pos_y = (i32)(window->height - dst_h) / 2;
    
    // cap max image size
    u64 image_size = (u64)dst_w * dst_h * sizeof(u32);

    if (image_size <= MB(30)) {
        app_state.main_image.width = dst_w;
        app_state.main_image.height = dst_h;
    } else {
        pos_x = (i32)(window->width - app_state.main_image.width) / 2;
        pos_y = (i32)(window->height - app_state.main_image.height) / 2;

        VERROR(
            "Image render size exceeds maximum: %llu bytes (maximum: %llu)\n",
            image_size,
            MB(1)
        );
    }

    // I should probably make blit_nearest and then later blit_the_interpolated(...,functor)
    image_resize_nearest(&app_state.original_buffer, app_state.main_image);
    blit(app_state.front_buffer,
        app_state.main_image, 
        pos_x, 
        pos_y 
    );

    platform_window_present_frame(window, (u8*)app_state.front_buffer.pixels);
}

void on_window_close(vwindow* window){
    VDEBUG("window closed.\n");
    app_state.is_running = false;
}

b8 application_create(application_state* application){
    // RESERVE MY ABSURD AMOUNT OF MEMORY THAT I MAY NEVER USE ALL
    // BUT IT IS VIRUTLE and NOT physical, so all good
    platform_virtual_reserve(&application->front_buffer_virtual, GB(1));
    //platform_virtual_reserve(&application->original_buffer_virtual, MB(300));
    platform_virtual_reserve(&application->main_image_virtual, GB(1));
    
    application->front_buffer.pixels = platform_virtual_commit(&application->front_buffer_virtual, MB(30));
    if(!application->front_buffer.pixels){
        VFATAL("Failed to allocated memory for front_buffer");
    }

    application->main_image.pixels = platform_virtual_commit(&application->main_image_virtual, MB(30));
    if(!application->main_image.pixels){
        VFATAL("Failed to allocated memory for main_image");
    }
        
    application->is_running = true;
        
    platform_set_window_close_callback(on_window_close);
    utf8_to_platform_string(WINDOW_TITLE, 0, 0);
    platform_string plf_window_title = (char[256]){};
    utf8_to_platform_string(WINDOW_TITLE, plf_window_title, 256); 
    
    if(!platform_window_create(plf_window_title, WINDOW_WIDTH, WINDOW_HEIGHT, 200, 200, &application->window)){
        VFATAL("Failed to Create Window");
    };
    VDEBUG("window created.\n");


    return true;
}

void application_start(application_state* application, char* image_name){
    // nearest niebore image resize needed.
    int image_channels = 4;
    app_state.original_buffer.pixels = (void*)stbi_load(image_name, &app_state.original_buffer.width, &app_state.original_buffer.height, &image_channels, 4);
    if(!app_state.original_buffer.pixels){
        VFATAL("Failed to load the image named %s\n", image_name);
        return;
    }
    VDEBUG("image named %s loaded with a width of %i and a height of %i\n", image_name, app_state.original_buffer.width, app_state.original_buffer.height);

    swizzle_rgba_to_bgra_horizontal(&app_state.original_buffer);
   
    app_state.front_buffer.width = WINDOW_WIDTH;
    app_state.front_buffer.height = WINDOW_HEIGHT;

    image_resize_nearest(&app_state.original_buffer, app_state.main_image);

    on_window_resize(&app_state.window);

    platform_set_window_resize_callback(on_window_resize);
}

void application_update(application_state* application){
    while (application->is_running) {
        platform_pump_messages();
    }
}

void application_destroy(application_state* application){
    platform_virtual_unreserve(&application->main_image_virtual);
    platform_virtual_unreserve(&application->front_buffer_virtual);
    platform_window_destroy(&application->window);
    stbi_image_free(application->original_buffer.pixels);
}

int app_main(int agrc, platform_string argv[])
{
    logger_initialize(1024, (char[1024]){});
    if (!platform_initialize()){
            VERROR("Failed to create initalize window\n");
            return EXIT_FAILURE;
    }{
        application_create(&app_state);
        u32 arg_byte_len = platform_string_to_utf8(argv[1], PNULL, 0);
        if(arg_byte_len == 0){
            VFATAL("usage prog <image-path>\n");
            platform_uninitalize();
            return 1;
        }

        char* image_name = platform_heap_allocate(arg_byte_len);
        if (!image_name){
            VFATAL("heap allocation failed\n");
            platform_uninitalize();
            return 1;
        }

        platform_string_to_utf8(argv[1], image_name, arg_byte_len);
        application_start(&app_state, image_name);

        application_update(&app_state);

        application_destroy(&app_state);
        platform_heap_deallocate(image_name);
    }
    platform_uninitalize();
    return EXIT_SUCCESS;
}