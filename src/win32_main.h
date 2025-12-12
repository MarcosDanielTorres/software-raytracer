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

typedef struct Software_Render_Buffer Software_Render_Buffer;
struct Software_Render_Buffer
{
    i32 width;
    i32 height;
    BITMAPINFO info;
    u32 *data;
};

#define foo(name) void name(Software_Render_Buffer *buffer)
typedef foo(Game_Update_And_Render);