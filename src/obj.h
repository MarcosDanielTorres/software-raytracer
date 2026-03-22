#pragma once
// TODO
// Size of allocations
// - Decide what to do with the 1-based indexing of things!
// - It seems very good to have an array structure instead of having to maintain a data and count for each
// fucking thing i want. I would be nice to have something like: texture_coordinates.data, texture_coordinates.size instead of
// texture_coordinates and texture_coordinates_count because I see myself forgetting the names of the variables and not being consistent
// as well


typedef struct Obj_Key Obj_Key;
struct Obj_Key
{
    i32 v;
    i32 vt;
    i32 vn;
};

typedef struct Obj_Vertex_Map_Entry Obj_Vertex_Map_Entry;
struct Obj_Vertex_Map_Entry
{
    Obj_Key key;
    u32 index_into_vertex_array;
    Obj_Vertex_Map_Entry *next;
};

typedef struct Obj_VertexMap Obj_VertexMap;
struct Obj_VertexMap
{
    Obj_Vertex_Map_Entry *table[256];
};

typedef struct Map_Result Map_Result;
struct Map_Result
{
    b32 created;
    u32 index_into_vertex_array;
};

typedef struct Face Face;
struct Face
{
    // vvv,vtvtvt,vnvnvn
    int packed_data[9];
    int v[3];
    int vt[3];
    int vn[3];
};

typedef struct Obj_Model Obj_Model;
struct Obj_Model
{
    Vec3 *vertices;
    u32 vertex_count;
    Vec2 *texture_coordinates;
    u32 texture_coordinates_count;
    Vec3 *normals;
    u32 normal_count;
    Face *faces;
    u32 face_count;
    b32 is_valid;
    b32 has_normals;
};

internal inline u64 hash_obj_key(Obj_Key k);
internal inline i32 obj_key_equal(Obj_Key a, Obj_Key b);

internal Mesh obj_build_mesh(Arena *arena, Obj_Model model);
internal Map_Result map_get_or_create(Arena *arena, Obj_VertexMap *map, Obj_Key key, u32 index);

inline b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
}

internal void obj_parser_eat_until_newline(char** at2);
internal f32 obj_parser_convert_cstring_to_f32(char *start, u32 *movement);
Obj_Model parse_obj(char *buf, size_t size);