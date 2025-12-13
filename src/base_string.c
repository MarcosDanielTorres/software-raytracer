internal String8
str8(u8* data, u64 len)
{
    String8 result = {data, len};
    return result;
}

internal String8
str8_strip_last(String8 str, String8 strip)
{
    if (strip.len > str.len) return str;
    for(u32 i = str.len - 1; i != -1; i-=strip.len)
    {
        b32 found = 1;
        for (u32 j = 0; j < strip.len; j++)
        {
            if (str.data[i - j] != strip.data[j])
            {
                found = 0;
                break;
            }
        }
        if (found)
        {
            str.len = i;
            break;
        }
    }
    return str;
}

internal String8
str8_concat(Arena *arena, String8 a, String8 b)
{
    String8 result = {0};
    u64 len = a.len + b.len;
    u8* data = arena_push_size(arena, u8, len);
    memcpy(data, a.data, a.len);
    memcpy(data + a.len, b.data, b.len);
    result.data = data;
    result.len = len;
    return result;
}

internal u8*
cstring_from_str8(Arena* arena, String8 str)
{
    u8* result = arena_push_size(arena, u8, str.len + 1);
    memcpy(result, str.data, str.len);
    memset(result + str.len, 0, 1);
    return result;
}

internal u64
cstring_length(u8* str)
{
    u8* ptr = str;
    u64 len = 0;
    while(*ptr++)
    {
        len++;
    }
    return len;
}

internal u8*
cstring_concat(Arena *arena, u8* a, u8* b)
{
    u8* result = 0;
    TempArena scratch = scratch_begin();
    u64 a_len = cstring_length(a);
    u64 b_len = cstring_length(b);
    u64 len = a_len + b_len + 1;
    u8* data = arena_push_size(scratch.arena, u8, len);
    u8* ptr = data;
    for(u32 i = 0; i < a_len; i++)
    {
        *ptr++ = a[i];
    }
    for(u32 i = 0; i < b_len; i++)
    {
        *ptr++ = b[i];
    }
    *ptr = '\0';

    result = arena_push_copy(arena, len, data)
    scratch_end(scratch);
    return result;
}