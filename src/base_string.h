#pragma once

typedef struct String8 String8;
struct String8
{
    u8* data;
    u64 len;
};

#define str8_literal(str) str8((u8*)str, sizeof(str) - 1)

internal String8 str8(u8* data, u64 len);
internal String8 str8_strip_last(String8 str, String8 strip);
internal String8 str8_concat(Arena *arena, String8 a, String8 b);
internal u8* cstring_from_str8(Arena* arena, String8 str);
internal u64 cstring_length(u8* str);
internal u8* cstring_concat(Arena *arena, u8* a, u8* b);