
#define arena_push_size(arena, value, count, ...) (value*) _push_size(arena, sizeof(value) * count, 8);
#define arena_push_copy(arena, size, source, ...) memcpy((void*) _push_size(arena, size, 8), source, size);
global Arena *g_transient_arena;
#if 1
// TODO think this through!
internal Arena *
arena_alloc(size_t size)
{
    u8 *base = (u8*)VirtualAlloc(0, size + sizeof(Arena), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Arena *arena = (Arena*)base;
    arena->base = base + sizeof(Arena);
    arena->len = 0;
    arena->max_len = size;
    arena->temp_count = 0;
    return arena;
}
#endif

internal void arena_init(Arena* arena, size_t size, u8* base) 
{
    if (base)
    {
        arena->base = base;
    }
    else
    {
        arena->base = (u8*)VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    arena->len = 0;
    arena->max_len = size;
    arena->temp_count = 0;
}

internal void* _push_size(Arena* arena, size_t size, u64 alignment)
{
    /*
        000000 
    */
    u64 result_ptr = (u64)arena->base + arena->len;
    u64 alignment_offset = 0;
    u64 alignment_mask = alignment - 1;
    if(result_ptr & alignment_mask)
    {
        // align
        alignment_offset = alignment - (result_ptr & alignment_mask);

    }
    size += alignment_offset;
    //AssertGui(arena->len + size <= arena->max_len, "Requested size: %zd | arena cap: %zd", arena->len + size, arena->max_len);
    if(arena->len + size > arena->max_len)
    {
        printf("FAILED Requested size: %zd | arena cap: %zd\n", arena->len + size, arena->max_len);
    }
    else
    {
        //printf("Requested size: %zd | arena cap: %zd\n", arena->len + size, arena->max_len);
    }
    arena->len += size;
    void* result = (void*)(result_ptr + alignment_offset);
    return result;
}


internal TempArena temp_begin(Arena* arena) 
{
    TempArena result = {0};
    result.arena = arena;
    result.pos = arena->len;
    arena->temp_count++;
    return result;
}

internal void temp_end(TempArena temp_arena) 
{
    temp_arena.arena->len = temp_arena.pos;
    temp_arena.arena->temp_count--;
}

internal TempArena
scratch_begin()
{
    TempArena scratch = temp_begin(g_transient_arena);
    return scratch;
}

internal void
scratch_end(TempArena temp)
{
    temp_end(temp);
}