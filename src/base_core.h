#pragma once
#include <stdint.h>
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

#define Min(a, b) (a < b ? (a) : (b))
#define Max(a, b) (a > b ? (a) : (b))
#define Sign(type, x) (((type)(x > 0)) - ((type)(x) < 0))
#define ClampBot(a, b) (Max(a, b))
#define ClampTop(a, b) (Min(a, b))
#define Clamp(a, b, c) (ClampTop(ClampBot(a, b), c))
#define ArrayCount(a) (sizeof(a) / sizeof(*a))