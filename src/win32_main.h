#pragma once
#include <stdint.h>
#include <windows.h>
#include <math.h>
#include <stdio.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef uint32_t b32;

#define global        static
#define internal      static
#define local_persist static

#define kb(value) (value * 1024)
#define mb(value) (kb(value) * 1024)
#define gb(value) (mb(value) * 1024)

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