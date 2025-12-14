#pragma once
typedef struct Software_Render_Buffer Software_Render_Buffer;
struct Software_Render_Buffer
{
    i32 width;
    i32 height;
    BITMAPINFO info;
    u32 *data;
};
#define UPDATE_AND_RENDER(name) void name(Software_Render_Buffer *buffer)
typedef UPDATE_AND_RENDER(Update_And_Render);
UPDATE_AND_RENDER(update_and_render_stub)
{

}