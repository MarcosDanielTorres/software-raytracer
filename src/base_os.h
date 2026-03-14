#pragma once
typedef struct OS_Handle OS_Handle;
struct OS_Handle
{
    u64 U64;
};

typedef struct Software_Render_Buffer Software_Render_Buffer;
typedef struct Software_Depth_Buffer Software_Depth_Buffer;
typedef struct Game_Input Game_Input;
typedef struct Game_Memory Game_Memory;
struct Game_Memory
{
    b32 init;
    void* persistent_memory;
    void* transient_memory;
    u64 persistent_memory_size;
    u64 transient_memory_size;
};

#define UPDATE_AND_RENDER(name) void name(Software_Render_Buffer *buffer, Software_Depth_Buffer *depth_buffer, Game_Memory *game_memory, Game_Input *input, f32 total_time, f32 dt)
typedef UPDATE_AND_RENDER(Update_And_Render);
UPDATE_AND_RENDER(update_and_render_stub)
{

}

typedef struct OS_LoadedDLL OS_LoadedDLL;
struct OS_LoadedDLL
{
    OS_Handle handle;
    u8* name;
    u8* name_temp;
    u8* lock_filename;
    Update_And_Render *app_update_and_render;
    u64 last_write_time;
    b32 is_valid;
};

typedef struct OS_FileReadResult OS_FileReadResult;
struct OS_FileReadResult
{
    u8* data;
    u64 size;
};

internal OS_FileReadResult os_file_read(Arena *arena, const char *filename);
internal void os_load_dll(OS_LoadedDLL *dll);
internal void os_unload_dll(OS_LoadedDLL *dll);
internal void os_perform_hot_reload(OS_LoadedDLL *dll);