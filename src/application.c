#include "application.h"
#include "core/defines.h"
#include "core/platform/platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include "image/stb_image.h"
#include "image/image_util.h"
#include "core/logger.h"
#include "core/allocators/arena.h"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 500
#define WINDOW_TITLE "Image Viewer"


typedef struct exapand_arena {
    arena base;
}exapand_arena;

b8 exapand_arena_expand(exapand_arena* arena, u64 extra)
{
    if(!arena){
        return false;
    }
    arena->base.memory_total += extra;
    return true;
}

typedef struct gpa_allocator {
    vvirtual_memory virtual_memory;
    exapand_arena allocation_strategy;
}gpa_allocator;

typedef struct application_state {
    gpa_allocator allocator;
    vwindow window;
    Image original_buffer;
    Image front_buffer;
    Image back_buffer;
    b8 is_running;
}application_state;

application_state app_state;

void on_window_resize(vwindow* window){
    if (window->height * window->width <= 1080 * 1920){
        platform_window_present_frame(window, (u8*)app_state.back_buffer.pixels,
            window->width, window->height, 
            0, 0);
    }
    platform_window_present_frame(window, (u8*)app_state.front_buffer.pixels, 
            app_state.front_buffer.width, app_state.front_buffer.height, 
            (window->width - app_state.front_buffer.width)/2, (window->height - app_state.front_buffer.height)/2);
    
}

void on_window_close(vwindow* window){
    VDEBUG("window closed\n");
    app_state.is_running = false;
}

void* application_arena_allocate(application_state* application, u64 memory_to_allocate){
    void* requested = arena_allocate(&application->allocator.allocation_strategy.base, memory_to_allocate);
    // arena broken
    if(!requested){
        VDEBUG("Arena broken. Requesting more memory.\n");
        requested = platform_virtual_commit(&application->allocator.virtual_memory, memory_to_allocate);
        if(!requested){
            VFATAL("Failed to allocate more memory");
            return PNULL;
        }
        //platform_copy_memory(requested, application->strategy.memory, application->strategy.memory_total);
        //arena_destroy(&application->allocator.allocation_strategy);
        //platform_heap_deallocate(application->strategy.memory);
        //arena_create(&application->allocator.allocation_strategy.base, requested, memory_to_allocate);
        exapand_arena_expand(&application->allocator.allocation_strategy, memory_to_allocate);
        VDEBUG("Arena Expanded.\n");
    }
    return requested;
}

b8 application_create(application_state* application){
    // RESERVE MY ABSURD AMOUNT OF MEMORY THAT I MAY NEVER USE ALL :)
    // BUT IT IS VIRUTLE and NOT physical, so all good
    platform_virtual_reserve(&application->allocator.virtual_memory, MB(300));

    void* mem = platform_virtual_commit(&application->allocator.virtual_memory, MB(1));
    if(!arena_create(&application->allocator.allocation_strategy.base, mem, MB(1))){
        return false;
    }
    application->is_running = true;

    platform_set_window_resize_callback(on_window_resize);
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
    // nearest neigbore image resize needed.
    int image_channels = 4;
    app_state.original_buffer.pixels = (void*)stbi_load(image_name, &app_state.original_buffer.width, &app_state.original_buffer.height, &image_channels, 4);
    
    // I guess why not do 2 things at once. Returns NULL anyway if no image
    // check if image is null
    // convert RGBA to BGRA
    if(!swizzle_rgba_to_bgra_horizontal((void*)app_state.original_buffer.pixels, app_state.original_buffer.width*app_state.original_buffer.height*sizeof(u32))){
        VFATAL("Failed to load the image named %s\n", image_name);
        return;
    }
    VDEBUG("image named %s loaded with a width of %i and a height of %i\n", image_name, app_state.original_buffer.width, app_state.original_buffer.height);
    
    Image src = {
      .width = app_state.original_buffer.width, .height = app_state.original_buffer.height, .pixels = app_state.original_buffer.pixels};

    u32 const new_width = WINDOW_WIDTH;
    u32 const new_height = WINDOW_HEIGHT;
    Image dst = {.width = new_width,
                 .height = new_height,
                 .pixels = application_arena_allocate(&app_state, new_width * new_height * sizeof(u32))};

    if (!dst.pixels) {
      printf("Failed to allocate resized image\n");
      stbi_image_free(app_state.original_buffer.pixels);
    }
    image_resize_nearest(&src, dst);
    app_state.front_buffer = dst;
    app_state.back_buffer.pixels = application_arena_allocate(&app_state,(1080 * 1920 * sizeof(u32)));
    if(!app_state.back_buffer.pixels){
        printf("Failed to allocate clear buffer\n");
    }
    app_state.back_buffer.height = 1920;
    app_state.back_buffer.width = 1080;

    platform_window_present_frame(&application->window, (u8*)application->front_buffer.pixels, 
        application->front_buffer.width, application->front_buffer.height, 
        (application->window.width - application->front_buffer.width)/2, (application->window.height - application->front_buffer.height)/2);
}

void application_update(application_state* application){
    while (application->is_running) {
        platform_pump_messages();
    }
}

void application_destroy(application_state* application){
    arena_destroy(&application->allocator.allocation_strategy.base);
    platform_virtual_unreserve(&application->allocator.virtual_memory);
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