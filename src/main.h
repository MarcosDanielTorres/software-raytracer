#pragma once
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
internal void inline clear_screen(Software_Render_Buffer *buffer, u32 color)
{
    u32 size = buffer->width * buffer->height;
    __m128i value = _mm_set1_epi32(color);

    u32 *buffer_ptr = buffer->data;
    for(u32 i = 0; i < size; i += 4)
    {
        _mm_store_si128((__m128i*)(buffer_ptr + i), value);
    }
}

internal void inline clear_depth_buffer(Software_Depth_Buffer *buffer)
{

    #if REVERSE_DEPTH_VALUE
    memset((void*)buffer->data, 0x00, buffer->width * buffer->height * 4);
    #else
    u32 size = buffer->width * buffer->height;
    __m128 value = _mm_set1_ps(max_f32);

    f32 *buffer_ptr = buffer->data;
    for(u32 i = 0; i < size; i += 4)
    {
        _mm_store_ps(buffer_ptr + i, value);
    }
    //memset(buffer->data, 0x7F, size * sizeof(f32));
    #endif
}