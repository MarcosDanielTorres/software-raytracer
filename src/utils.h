#pragma once

global Vec3 red = {255, 0, 0};
global Vec3 green = {0, 255, 0};
global Vec3 blue = {0, 0, 255};
global Vec3 white = {255, 255, 255};
global Vec3 yellow = {255, 255, 0};

typedef struct Vertex Vertex;
struct Vertex
{
    Vec3 position;
    Vec3 color;
    Vec2 uv;
    Vec3 normal;
};

typedef struct Mesh Mesh;
struct Mesh 
{
    u32 *indices;
    Vertex *vertices;
    u32 indices_count;
    u32 vertices_count;
    u32* texture_data;
    u32 texture_width;
    u32 texture_height;
};