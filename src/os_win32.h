#pragma once
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

// --- win32 ---
internal WIN32_FIND_DATAA win32_get_file_attributes(u8* filename);
internal u64 win32_filetime_to_u64(FILETIME time);


internal void os_load_dll(OS_LoadedDLL *dll);
internal void os_unload_dll(OS_LoadedDLL *dll);
internal void os_perform_hot_reload(OS_LoadedDLL *dll);
