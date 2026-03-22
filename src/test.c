#include "base_core.h"
#include "base_arena.h"
#include "base_os.h"
#include "base_string.h"
#include "input.h"
#include "os_win32.h"
#include "timer.h"
#include "base_arena.c"
#include "os_win32.c"
#include "base_math.h"
#include "utils.h"
#include "timer.c"
#include "obj.h"
#include "obj.c"
#include <assert.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#include "base_string.c"
#include "main.h"
//#include "font_loader.h"
//#include "font_loader.c"
#undef internal
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"


typedef struct Camera Camera;
struct Camera
{
    Vec3 position;
    Vec3 forward;
    Vec3 right;
    f32 pitch;
    f32 yaw;
};

typedef struct Bitmap Bitmap;
struct Bitmap {
    i32 width;
    i32 height;
    i32 pitch;
    u8* buffer;
};

typedef struct FontGlyph FontGlyph;
struct FontGlyph {
    Bitmap bitmap;
    i32 bitmap_top;
    i32 bitmap_left;
    i32 advance_x;

    f32 uv0_x;
    f32 uv1_x;
    f32 uv2_x;
    f32 uv3_x;

    f32 uv0_y;
    f32 uv1_y;
    f32 uv2_y;
    f32 uv3_y;
};

typedef struct FontInfo FontInfo;
struct FontInfo
{
    i32 ascent;
    i32 descent;
    i32 line_height;
    i32 max_glyph_width;
    i32 max_glyph_height;
    // probably renamed to max_char_advancement (same in monospace)
    i32 max_char_width;
    FontGlyph font_table[300];
};



typedef struct Entity Entity;
struct Entity
{
    Mesh mesh;
};

typedef struct Game_State Game_State;
struct Game_State
{
    Arena *arena;
    FontInfo font_info;
    Camera camera;

    u32 example;
    u32 example_count;
    u32 example12_lighting_mode;
    u32 culled_triangles;

    Vertex *terrain_vertices;
    u32 terrain_vertices_count;
    u32 terrain_indices_count;
    u32 *terrain_indices;
    
    Mesh mesh_f117;
    Mesh mesh_efa;
    Mesh mesh_f22;
};

typedef u32 Example12LightingMode;
enum
{
    Example12Lighting_Unlit,
    Example12Lighting_Flat,
    Example12Lighting_Gouraud,
    Example12Lighting_Count,
};

typedef u32 RendererProperties_Flags;
enum
{
    RenderFlags_AffineUVInterpolation = (1u << 0),
    RenderFlags_Culling = (1u << 1),
    RenderFlags_TwoSidedRasterization = (1u << 2),
};

typedef struct Params Params;
struct Params
{
    Mat4 view;
    Mat4 persp;
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
    Vec3 v0_color;
    Vec3 v1_color;
    Vec3 v2_color;
    Vec2 v0_uv;
    Vec2 v1_uv;
    Vec2 v2_uv;
    u32 *texture;
    u32 texture_width;
    u32 texture_height;
    f32 inv_w0;
    f32 inv_w1;
    f32 inv_w2;
    f32 min_x;
    f32 max_x;
    f32 min_y;
    f32 max_y;
    Obj_Model *model;
    Software_Render_Buffer *buffer;
    Software_Depth_Buffer *depth_buffer;
    RendererProperties_Flags render_flags;
};

typedef struct Triangle Triangle;
struct Triangle
{
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
    String8 text;
    f32 sign_area;
};

internal u8
clamp_u8_from_f32(f32 value)
{
    if(value < 0.0f) return 0;
    if(value > 255.0f) return 255;
    return (u8)(value + 0.5f);
}

internal u32
multiply_color_u32_by_rgb8(u32 color, u8 tint_r, u8 tint_g, u8 tint_b)
{
    u8 a = (u8)((color >> 24) & 0xFF);
    u32 src_r = (color >> 16) & 0xFF;
    u32 src_g = (color >> 8) & 0xFF;
    u32 src_b = (color >> 0) & 0xFF;
    u32 r = (src_r * tint_r + 127) / 255;
    u32 g = (src_g * tint_g + 127) / 255;
    u32 b = (src_b * tint_b + 127) / 255;

    u32 result = 0;
    result |= ((u32)a) << 24;
    result |= r << 16;
    result |= g << 8;
    result |= b << 0;
    return result;
}

internal f32
saturate_f32(f32 value)
{
    if(value < 0.0f) return 0.0f;
    if(value > 1.0f) return 1.0f;
    return value;
}

internal Vec3
vec3_lerp(Vec3 a, Vec3 b, f32 t)
{
    t = saturate_f32(t);
    return (Vec3)
    {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

internal Vec3
color_scale(Vec3 color, f32 scale)
{
    return (Vec3){color.x * scale, color.y * scale, color.z * scale};
}

internal f32
ps1_grass_height(f32 x, f32 z)
{
    f32 rolling = 1.35f + 0.22f * sinf(x * 0.42f) + 0.18f * cosf(z * 0.31f);
    f32 ripple = 0.07f * sinf((x + z) * 0.35f);
    f32 hill_dx = x + 3.5f;
    f32 hill_dz = z - 15.0f;
    f32 hill = 0.55f * expf(-(hill_dx * hill_dx + hill_dz * hill_dz) * 0.035f);
    return rolling + ripple + hill;
}


internal Vec3
ps1_grass_base_color(f32 x, f32 z, f32 y)
{
    f32 wave = 0.5f + 0.5f * sinf(x * 0.18f + z * 0.11f);
    f32 height_factor = saturate_f32((y - 1.0f) / 1.2f);
    Vec3 dark = {64.0f, 138.0f, 56.0f};
    Vec3 bright = {103.0f, 186.0f, 82.0f};
    Vec3 color = vec3_lerp(dark, bright, 0.30f + wave * 0.45f);
    color = vec3_lerp(color, (Vec3){142.0f, 202.0f, 88.0f}, height_factor * 0.25f);
    return color;
}

internal Vec3
shade_directional(Vec3 base_color, Vec3 normal, Vec3 light_dir, f32 ambient, f32 diffuse)
{
    f32 intensity = ambient + diffuse * saturate_f32(vec3_dot(normal, light_dir));
    return color_scale(base_color, intensity);
}

internal f32
hash01(u32 seed)
{
    f32 value = sinf((f32)seed * 12.9898f + 78.233f) * 43758.5453f;
    return value - floorf(value);
}

internal f32
ease_out_cubic(f32 t)
{
    t = saturate_f32(t);
    f32 inv_t = 1.0f - t;
    return 1.0f - inv_t * inv_t * inv_t;
}

internal f32
smoothstep_f32(f32 edge0, f32 edge1, f32 x)
{
    if(equal_f32(edge0, edge1))
    {
        return x >= edge1;
    }

    f32 t = saturate_f32((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

internal char *
example12_lighting_mode_name(Example12LightingMode mode)
{
    char *result = "Unlit";
    if(mode == Example12Lighting_Flat) result = "Flat";
    if(mode == Example12Lighting_Gouraud) result = "Gouraud";
    return result;
}

global FontInfo font_info;
internal void draw_text(Software_Render_Buffer *buffer, u32 x, u32 baseline, char *text)
{
    u32 pen_x = x;
    for(char *c = text; *c != '\0'; c++)
    {
        FontGlyph glyph = font_info.font_table[(u32)*c];
        u32 width = glyph.bitmap.width;
        u32 height = glyph.bitmap.height;
        u32 *ptr = buffer->data + buffer->width * (baseline - glyph.bitmap_top) + pen_x;
        for(u32 y = 0; y < height; y++)
        {
            u32 *row = ptr;
            for(u32 x = 0; x < width; x++)
            {
                u8 *source = glyph.bitmap.buffer + width * y + x;
                #if 0
                u32 alpha = *source << 24; 
                u32 red = *source << 16; 
                u32 green = *source << 8; 
                u32 blue = *source; 
                u32 color = alpha | red | green | blue;
                *row++ = color;
                #else

                f32 sa = (f32)(*source / 255.0f);

                u32 color = 0xFFFFFFFF;

                f32 sr = (f32)((color >> 16) & 0xFF);
                f32 sg = (f32)((color >> 8) & 0xFF);
                f32 sb = (f32)((color >> 0) & 0xFF);

                f32 dr = (f32)((*row >> 16) & 0xFF);
                f32 dg = (f32)((*row >> 8) & 0xFF);
                f32 db = (f32)((*row >> 0) & 0xFF);
                u32 nr = (u32)((1.0f - sa) * dr + sa*sr);
                u32 ng = (u32)((1.0f - sa) * dg + sa*sg);
                u32 nb = (u32)((1.0f - sa) * db + sa*sb);

                *row++ = (nr << 16) | (ng << 8) | (nb << 0);
                #endif
            }
            ptr += buffer->width;
        }
        pen_x += font_info.max_char_width >> 6;
    }
}

internal inline void draw_pixel(Software_Render_Buffer *buffer, f32 x, f32 y, u32 color)
{
#if 0
	u32 x_min = (u32)roundf(x);
	u32 y_min = (u32)roundf(y);
#else
    // if i dont do this it overflows the integer when converting it back to u32
    if (x < 0) x = 0;
    if (y < 0) y = 0;
	u32 x_min = x;
	u32 y_min = y;
#endif
#if 1
   // if (x_min < 0) x_min = 0;
    if (x_min >= buffer->width - 1) x_min = buffer->width - 1;
    //if (y_min < 0) y_min = 0;
    if (y_min >= buffer->height - 1) y_min = buffer->height - 1;
#else
    if (x_min < 0) return;
    if (x_min > buffer->width - 1) return;
    if (y_min < 0) return;
    if (y_min > buffer->height - 1) return;
#endif
    
	u32 *ptr = buffer->data + y_min * buffer->width + x_min;
#if 0
    *ptr = color;
#else
    memcpy((void*)ptr, &color, 4);
#endif
}
static inline float edge(Vec2F32 a, Vec2F32 b, Vec2F32 p)
{
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

static inline b32 is_top_left(Vec2F32 a, Vec2F32 b)
{
    return (a.y > b.y) || (a.y == b.y && a.x < b.x);
}

internal b32
triangle_screen_bounds(Software_Render_Buffer *buffer, Vec3 v0, Vec3 v1, Vec3 v2,
                       f32 *min_x, f32 *max_x, f32 *min_y, f32 *max_y)
{
    if(!isfinite(v0.x) || !isfinite(v0.y) || !isfinite(v0.z) ||
       !isfinite(v1.x) || !isfinite(v1.y) || !isfinite(v1.z) ||
       !isfinite(v2.x) || !isfinite(v2.y) || !isfinite(v2.z))
    {
        return 0;
    }

    f32 raw_min_x = Min(Min(v0.x, v1.x), v2.x);
    f32 raw_min_y = Min(Min(v0.y, v1.y), v2.y);
    f32 raw_max_x = Max(Max(v0.x, v1.x), v2.x);
    f32 raw_max_y = Max(Max(v0.y, v1.y), v2.y);

    if(raw_max_x <= 0.0f || raw_min_x >= (f32)buffer->width ||
       raw_max_y <= 0.0f || raw_min_y >= (f32)buffer->height)
    {
        return 0;
    }

    *min_x = Clamp(raw_min_x, 0.0f, (f32)buffer->width);
    *max_x = Clamp(raw_max_x, 0.0f, (f32)buffer->width);
    *min_y = Clamp(raw_min_y, 0.0f, (f32)buffer->height);
    *max_y = Clamp(raw_max_y, 0.0f, (f32)buffer->height);

    return (*min_x < *max_x) && (*min_y < *max_y);
}

internal void barycentric_with_edge_stepping(Params *params)
{
    RendererProperties_Flags render_flags = params->render_flags;
    Vec3 v0 = params->v0;
    Vec3 v1 = params->v1;
    Vec3 v2 = params->v2;
    Vec3 v0_color = params->v0_color;
    Vec3 v1_color = params->v1_color;
    Vec3 v2_color = params->v2_color;
    Vec2 v0_uv = params->v0_uv;
    Vec2 v1_uv = params->v1_uv;
    Vec2 v2_uv = params->v2_uv;
    f32 inv_w0 = params->inv_w0;
    f32 inv_w1 = params->inv_w1;
    f32 inv_w2 = params->inv_w2;
    f32 min_x = params->min_x;
    f32 max_x = params->max_x;
    f32 min_y = params->min_y;
    f32 max_y = params->max_y;

    Vec2F32 s0 = { v0.x , (v0.y) };
    Vec2F32 s1 = { (v1.x), (v1.y) };
    Vec2F32 s2 = { (v2.x), (v2.y) };
    f32 area = edge(s0, s1, s2);          // signed
    if(!isfinite(area) || fabsf(area) <= EPSILON)
    {
        return;
    }

    f32 inv_area = 1.0f / area;
    // edge stepping basically
    // For E(a,b,p): dE/dx = (b.y - a.y), dE/dy = -(b.x - a.x)
    f32 e0_dx = (s2.y - s1.y);  float e0_dy = -(s2.x - s1.x); // E0 = edge(s1,s2,p)
    f32 e1_dx = (s0.y - s2.y);  float e1_dy = -(s0.x - s2.x); // E1 = edge(s2,s0,p)
    f32 e2_dx = (s1.y - s0.y);  float e2_dy = -(s1.x - s0.x); // E2 = edge(s0,s1,p)

    u32 x_min = (u32)floorf(min_x);
    u32 x_max = (u32)ceilf(max_x);
    u32 y_min = (u32)floorf(min_y);
    u32 y_max = (u32)ceilf(max_y);

    // Edge values must start at the same pixel center the loops begin from.
    f32 start_x = (f32)x_min + 0.5f;
    f32 start_y = (f32)y_min + 0.5f;
    Vec2F32 p0 = { start_x, start_y };
    f32 row_w0 = edge(s1, s2, p0);
    f32 row_w1 = edge(s2, s0, p0);
    f32 row_w2 = edge(s0, s1, p0);

    i32 e0_inc = is_top_left(s1, s2);
    i32 e1_inc = is_top_left(s2, s0);
    i32 e2_inc = is_top_left(s0, s1);

    const f32 eps = -1e-4f;
    int lx, hx, ly, hy;
    {
        for (u32 y = y_min; y < y_max; y++)
        {
            f32 w0 = row_w0;
            f32 w1 = row_w1;
            f32 w2 = row_w2;
            f32 *depth_row = params->depth_buffer->data + params->depth_buffer->width * y + x_min;
            u32 *color_row = params->buffer->data + params->buffer->width * y + x_min;
            for (u32 x = x_min; x < x_max; x++)
            {
                #if 1
                b32 inside_pos =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);
                #endif
                b32 inside = inside_pos;
                if(render_flags & RenderFlags_TwoSidedRasterization)
                {
                    b32 inside_neg =
                        (e0_inc ? w0 <= 0.f : w0 < -eps) &&
                        (e1_inc ? w1 <= 0.f : w1 < -eps) &&
                        (e2_inc ? w2 <= 0.f : w2 < -eps);
                    inside = inside_pos || inside_neg;
                }
                
                if (inside)
                {
                    f32 b0 = w0 * inv_area;
                    f32 b1 = w1 * inv_area;
                    f32 b2 = w2 * inv_area;
                    f32 inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;
                    //f32 depth = (b0 * v0.z + b1 * v1.z + b2 * v2.z) / inv_w_interp;
                    f32 depth = (b0 * v0.z + b1 * v1.z + b2 * v2.z);
                    //if(depth < params->depth_buffer->data[y * params->depth_buffer->width + x])
                    if(depth < depth_row[x - x_min])
                    {
                        // params->depth_buffer->data[y * params->depth_buffer->width + x] = depth;
                        depth_row[x - x_min] = depth;
                        f32 inv_inv_w = 1.0f / inv_w_interp;

                        f32 uv_x = (v0_uv.x * b0 + v1_uv.x * b1 + v2_uv.x * b2);
                        f32 uv_y = (v0_uv.y * b0 + v1_uv.y * b1 + v2_uv.y * b2);
                        if((render_flags & RenderFlags_AffineUVInterpolation) == 0)
                        {
                            uv_x *= inv_inv_w;
                            uv_y *= inv_inv_w;
                        }

                        f32 color_r = (v0_color.x * b0 + v1_color.x * b1 + v2_color.x * b2) * inv_inv_w;
                        f32 color_g = (v0_color.y * b0 + v1_color.y * b1 + v2_color.y * b2) * inv_inv_w;
                        f32 color_b = (v0_color.z * b0 + v1_color.z * b1 + v2_color.z * b2) * inv_inv_w;

                        u8 tint_r = clamp_u8_from_f32(color_r);
                        u8 tint_g = clamp_u8_from_f32(color_g);
                        u8 tint_b = clamp_u8_from_f32(color_b);

                        u32 out_color;
                        if(params->texture)
                        {
                            u32 tx = (u32)(uv_x * params->texture_width);
                            u32 ty = (u32)(uv_y * params->texture_height);
                            if(tx >= params->texture_width)  tx = params->texture_width - 1;
                            if(ty >= params->texture_height) ty = params->texture_height - 1;

                            u32 texel_index = ty * params->texture_width + tx;
                            out_color = multiply_color_u32_by_rgb8(params->texture[texel_index], tint_r, tint_g, tint_b);
                        }
                        else
                        {
                            out_color = (0xFFu << 24) | ((u32)tint_r << 16) | ((u32)tint_g << 8) | ((u32)tint_b << 0);
                        }
                        draw_pixel(params->buffer, x, y, out_color);
                    }
                }
                w0 += e0_dx; w1 += e1_dx; w2 += e2_dx; // step right
            }
            row_w0 += e0_dy; row_w1 += e1_dy; row_w2 += e2_dy; // step down
        }
    }
}

void provisionary_block(Software_Render_Buffer *buffer, Software_Depth_Buffer *depth_buffer, Triangle* triangles, u32 count, Mat4 view, Mat4 persp, RendererProperties_Flags render_flags)
{
    for(u32 i = 0; i < count; i++)
    {
        Triangle triangle = triangles[i];
        Vec3 v0 = triangle.v0;
        Vec3 v1 = triangle.v1;
        Vec3 v2 = triangle.v2;
        
        ////// view matrix ///////
        Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
        Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
        Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
        Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};
        Vec3 transformed_v1_v3 = (Vec3) {transformed_v1.x, transformed_v1.y, transformed_v1.z};
        Vec3 transformed_v2_v3 = (Vec3) {transformed_v2.x, transformed_v2.y, transformed_v2.z};
        ////// view matrix ///////

        ////// projection matrix ///////
        f32 inv_w0;
        f32 inv_w1;
        f32 inv_w2;
        // remember that the projection matrix stores de viewspace z value in its w
        // but the result vector is in clip space so z of the view matrix is in clip space, not in 
        // viewspace, they are not the same zz
        //  The near plane of the perspective matrix is 1 and the far plane is 50
        transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
        transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
        transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
        // Clip here, in clip space and before the perspective divide. Triangles fully outside
        // the frustum can be rejected here, and triangles crossing the near plane should be
        // split/intersected here; dividing first can make vertices behind the camera look valid.
        // Clipping
        if(transformed_v0.w < 0.1 || transformed_v1.w < 0.1 || transformed_v2.w < 0.1)
        {
            //printf("clipping triangle %d\n", i);
            continue;
        }
        // Clipping


        if(transformed_v0.w != 0)
        {
            inv_w0 = 1.0f / transformed_v0.w;
            transformed_v0.x *= inv_w0;
            transformed_v0.y *= inv_w0;
            transformed_v0.z *= inv_w0;
        }
        
        if(transformed_v1.w != 0)
        {
            inv_w1 = 1.0f / transformed_v1.w;
            transformed_v1.x *= inv_w1;
            transformed_v1.y *= inv_w1;
            transformed_v1.z *= inv_w1;
        }
        
        if(transformed_v2.w != 0)
        {
            inv_w2 = 1.0f / transformed_v2.w;
            transformed_v2.x *= inv_w2;
            transformed_v2.y *= inv_w2;
            transformed_v2.z *= inv_w2;
        }

        v0.x = transformed_v0.x;
        v0.y = transformed_v0.y;
        v0.z = transformed_v0.z;

        v1.x = transformed_v1.x;
        v1.y = transformed_v1.y;
        v1.z = transformed_v1.z;

        v2.x = transformed_v2.x;
        v2.y = transformed_v2.y;
        v2.z = transformed_v2.z;
        ////// projection matrix ///////

        // @Performance This could be done before or after the viewport transform as long as the viewport doesnt flip Y
        f32 sign_area = orient_2d_v3(v0, v1, v2);
        if((render_flags & RenderFlags_Culling) && sign_area > 0)
        {
            printf("Culling triangle %d with sign area: %.2f\n", i, sign_area);
            continue;
        }

        ////// viewport transform //////
        v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
        v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
        v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
        v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;
        v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;
        v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;
        ////// viewport transform //////

        f32 min_x;
        f32 min_y;
        f32 max_x;
        f32 max_y;
        if(!triangle_screen_bounds(buffer, v0, v1, v2, &min_x, &max_x, &min_y, &max_y))
        {
            continue;
        }

        Vec3 new_vv0_color = vec3_scalar((Vec3){255.0f, 0.0f, 0.0f}, inv_w0);
        Vec3 new_vv1_color = vec3_scalar((Vec3){0.0f, 255.0f, 0.0f}, inv_w1);
        Vec3 new_vv2_color = vec3_scalar((Vec3){0.0f, 0.0f, 255.0f}, inv_w2);

        Params params = 
        {
            view, persp,
            v0, v1, v2,
            new_vv0_color, new_vv1_color, new_vv2_color,
            {0,0}, {0,0}, {0,0}, 0, 0, 0,
            inv_w0, inv_w1, inv_w2,
            min_x, max_x, min_y, max_y
        };

        params.buffer = buffer;
        params.depth_buffer = depth_buffer;
        params.render_flags = render_flags;
        barycentric_with_edge_stepping(&params);
    }
}


internal void
push_triangle(Vertex *vertices, u32 max_vertices, u32 *vertex_count,
              Vec3 p0, Vec3 p1, Vec3 p2,
              Vec3 c0, Vec3 c1, Vec3 c2)
{
    assert((*vertex_count + 3) <= max_vertices);
    vertices[*vertex_count + 0] = (Vertex){.position = p0, .color = c0, .uv = {0, 0}};
    vertices[*vertex_count + 1] = (Vertex){.position = p1, .color = c1, .uv = {0, 0}};
    vertices[*vertex_count + 2] = (Vertex){.position = p2, .color = c2, .uv = {0, 0}};
    *vertex_count += 3;
}

internal void
push_flat_triangle(Vertex *vertices, u32 max_vertices, u32 *vertex_count,
                   Vec3 p0, Vec3 p1, Vec3 p2,
                   Vec3 base_color, Vec3 light_dir, f32 ambient, f32 diffuse)
{
    Vec3 normal = vec3_normalize(vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0)));
    Vec3 color = shade_directional(base_color, normal, light_dir, ambient, diffuse);
    push_triangle(vertices, max_vertices, vertex_count, p0, p1, p2, color, color, color);
}

internal void
push_flat_quad(Vertex *vertices, u32 max_vertices, u32 *vertex_count,
               Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3,
               Vec3 base_color, Vec3 light_dir, f32 ambient, f32 diffuse)
{
    push_flat_triangle(vertices, max_vertices, vertex_count, p0, p1, p2, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, p0, p2, p3, base_color, light_dir, ambient, diffuse);
}

internal void
push_flat_octahedron(Vertex *vertices, u32 max_vertices, u32 *vertex_count,
                     Vec3 center, Vec3 radii, Vec3 base_color,
                     Vec3 light_dir, f32 ambient, f32 diffuse)
{
    Vec3 top = {center.x, center.y - radii.y, center.z};
    Vec3 bottom = {center.x, center.y + radii.y, center.z};
    Vec3 east = {center.x + radii.x, center.y, center.z};
    Vec3 west = {center.x - radii.x, center.y, center.z};
    Vec3 north = {center.x, center.y, center.z + radii.z};
    Vec3 south = {center.x, center.y, center.z - radii.z};

    push_flat_triangle(vertices, max_vertices, vertex_count, top, east, north, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, top, north, west, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, top, west, south, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, top, south, east, base_color, light_dir, ambient, diffuse);

    push_flat_triangle(vertices, max_vertices, vertex_count, bottom, north, east, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, bottom, west, north, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, bottom, south, west, base_color, light_dir, ambient, diffuse);
    push_flat_triangle(vertices, max_vertices, vertex_count, bottom, east, south, base_color, light_dir, ambient, diffuse);
}

internal u32
build_ps1_terrain_patch(Vertex *vertices, u32 max_vertices,
                        u32 columns, u32 rows, f32 tile_width, f32 tile_depth,
                        f32 start_x, f32 start_z,
                        Example12LightingMode lighting_mode,
                        Vec3 light_dir, f32 ambient, f32 diffuse)
{
    u32 vertex_count = 0;
    for(u32 row = 0; row < rows; row++)
    {
        for(u32 column = 0; column < columns; column++)
        {
            f32 x0 = start_x + (f32)column * tile_width;
            f32 x1 = x0 + tile_width;
            f32 z0 = start_z + (f32)row * tile_depth;
            f32 z1 = z0 + tile_depth;

            Vec3 p00 = {x0, ps1_grass_height(x0, z0), z0};
            Vec3 p10 = {x1, ps1_grass_height(x1, z0), z0};
            Vec3 p01 = {x0, ps1_grass_height(x0, z1), z1};
            Vec3 p11 = {x1, ps1_grass_height(x1, z1), z1};

            Vec3 tri0_center = color_scale(vec3_add(vec3_add(p00, p10), p01), 1.0f / 3.0f);
            Vec3 tri1_center = color_scale(vec3_add(vec3_add(p10, p11), p01), 1.0f / 3.0f);
            Vec3 tri0_base = ps1_grass_base_color(tri0_center.x, tri0_center.z, tri0_center.y);
            Vec3 tri1_base = ps1_grass_base_color(tri1_center.x, tri1_center.z, tri1_center.y);
            push_flat_triangle(vertices, max_vertices, &vertex_count, p00, p10, p01, tri0_base, light_dir, ambient, diffuse);
            push_flat_triangle(vertices, max_vertices, &vertex_count, p10, p11, p01, tri1_base, light_dir, ambient, diffuse);
        }
    }

    return vertex_count;
}

internal void
rotate_vertices_about_center(Vertex *vertices, u32 count, Vec3 center, f32 angle_y, f32 angle_x)
{
    f32 cy = cosf(angle_y);
    f32 sy = sinf(angle_y);
    f32 cx = cosf(angle_x);
    f32 sx = sinf(angle_x);
    for(u32 vertex_index = 0; vertex_index < count; vertex_index++)
    {
        Vec3 p = vec3_sub(vertices[vertex_index].position, center);
        p = vec3_rotate_y(p, cy, sy);
        p = vec3_rotate_x(p, cx, sx);
        vertices[vertex_index].position = vec3_add(p, center);
    }
}

void provisionary_block2(Game_State *game_state, Software_Render_Buffer *buffer, Software_Depth_Buffer *depth_buffer, Vertex* vertices, u32 vertices_count, u32* indices, u32 indices_count, u32 *texels, u32 texels_width, u32 texels_height, Mat4 view, Mat4 persp, RendererProperties_Flags render_flags)
{
    if (indices)
    {
        for(u32 i = 0; i < indices_count; i+=3)
        {
            Vertex vertex0 = vertices[indices[i]];
            Vertex vertex1 = vertices[indices[i + 1]];
            Vertex vertex2 = vertices[indices[i + 2]];

            Vec3 v0 = vertex0.position;
            Vec3 v1 = vertex1.position;
            Vec3 v2 = vertex2.position;

            Vec2 v0_uv = vertex0.uv;
            Vec2 v1_uv = vertex1.uv;
            Vec2 v2_uv = vertex2.uv;

            Triangle triangle = {v0, v1, v2};
            
            ////// view matrix ///////
            Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
            Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
            Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
            Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};
            Vec3 transformed_v1_v3 = (Vec3) {transformed_v1.x, transformed_v1.y, transformed_v1.z};
            Vec3 transformed_v2_v3 = (Vec3) {transformed_v2.x, transformed_v2.y, transformed_v2.z};
            ////// view matrix ///////

            ////// projection matrix ///////
            f32 inv_w0;
            f32 inv_w1;
            f32 inv_w2;
            // remember that the projection matrix stores de viewspace z value in its w
            // but the result vector is in clip space so z of the view matrix is in clip space, not in 
            // viewspace, they are not the same zz
            //  The near plane of the perspective matrix is 1 and the far plane is 50
            transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
            transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
            transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
            // Clip here, in clip space and before the perspective divide. Triangles fully outside
            // the frustum can be rejected here, and triangles crossing the near plane should be
            // split/intersected here; dividing first can make vertices behind the camera look valid.
            // Clipping
            if(transformed_v0.w < 0.1 || transformed_v1.w < 0.1 || transformed_v2.w < 0.1)
            {
                //printf("clipping triangle %d\n", i);
                continue;
            }
            // Clipping


            if(transformed_v0.w != 0)
            {
                inv_w0 = 1.0f / transformed_v0.w;
                transformed_v0.x *= inv_w0;
                transformed_v0.y *= inv_w0;
                transformed_v0.z *= inv_w0;
            }
            
            if(transformed_v1.w != 0)
            {
                inv_w1 = 1.0f / transformed_v1.w;
                transformed_v1.x *= inv_w1;
                transformed_v1.y *= inv_w1;
                transformed_v1.z *= inv_w1;
            }
            
            if(transformed_v2.w != 0)
            {
                inv_w2 = 1.0f / transformed_v2.w;
                transformed_v2.x *= inv_w2;
                transformed_v2.y *= inv_w2;
                transformed_v2.z *= inv_w2;
            }

            v0.x = transformed_v0.x;
            v0.y = transformed_v0.y;
            v0.z = transformed_v0.z;

            v1.x = transformed_v1.x;
            v1.y = transformed_v1.y;
            v1.z = transformed_v1.z;

            v2.x = transformed_v2.x;
            v2.y = transformed_v2.y;
            v2.z = transformed_v2.z;
            ////// projection matrix ///////

            // @Performance This could be done before or after the viewport transform as long as the viewport doesnt flip Y
            f32 sign_area = orient_2d_v3(v0, v1, v2);
            if((render_flags & RenderFlags_Culling) && sign_area > 0)
            {
                game_state->culled_triangles++;
                continue;
            }

            ////// viewport transform //////
            v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
            v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
            v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
            v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;
            v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;
            v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;
            ////// viewport transform //////

            f32 min_x;
            f32 min_y;
            f32 max_x;
            f32 max_y;
            if(!triangle_screen_bounds(buffer, v0, v1, v2, &min_x, &max_x, &min_y, &max_y))
            {
                continue;
            }

            Vec3 new_vv0_color = vec3_scalar(vertex0.color, inv_w0);
            Vec3 new_vv1_color = vec3_scalar(vertex1.color, inv_w1);
            Vec3 new_vv2_color = vec3_scalar(vertex2.color, inv_w2);
            if((render_flags & RenderFlags_AffineUVInterpolation) == 0)
            {
                v0_uv = vec2_scalar(v0_uv, inv_w0);
                v1_uv = vec2_scalar(v1_uv, inv_w1);
                v2_uv = vec2_scalar(v2_uv, inv_w2);
            }

            Params params = 
            {
                view, persp,
                v0, v1, v2,
                new_vv0_color, new_vv1_color, new_vv2_color,
                v0_uv, v1_uv, v2_uv, texels, texels_width, texels_height,
                inv_w0, inv_w1, inv_w2,
                min_x, max_x, min_y, max_y
            };

            params.buffer = buffer;
            params.depth_buffer = depth_buffer;
            params.render_flags = render_flags;
            barycentric_with_edge_stepping(&params);
        }
    }
    else 
    {
        for(u32 i = 0; i < vertices_count; i+=3)
        {
            Vertex vertex0 = vertices[i];
            Vertex vertex1 = vertices[i + 1];
            Vertex vertex2 = vertices[i + 2];

            Vec3 v0 = vertex0.position;
            Vec3 v1 = vertex1.position;
            Vec3 v2 = vertex2.position;

            Vec2 v0_uv = vertex0.uv;
            Vec2 v1_uv = vertex1.uv;
            Vec2 v2_uv = vertex2.uv;

            Triangle triangle = {v0, v1, v2};
            
            ////// view matrix ///////
            Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
            Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
            Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
            Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};
            Vec3 transformed_v1_v3 = (Vec3) {transformed_v1.x, transformed_v1.y, transformed_v1.z};
            Vec3 transformed_v2_v3 = (Vec3) {transformed_v2.x, transformed_v2.y, transformed_v2.z};
            ////// view matrix ///////

            ////// projection matrix ///////
            f32 inv_w0;
            f32 inv_w1;
            f32 inv_w2;
            // remember that the projection matrix stores de viewspace z value in its w
            // but the result vector is in clip space so z of the view matrix is in clip space, not in 
            // viewspace, they are not the same zz
            //  The near plane of the perspective matrix is 1 and the far plane is 50
            transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
            transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
            transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
            // Clip here, in clip space and before the perspective divide. Triangles fully outside
            // the frustum can be rejected here, and triangles crossing the near plane should be
            // split/intersected here; dividing first can make vertices behind the camera look valid.
            // Clipping
            if(transformed_v0.w < 0.1 || transformed_v1.w < 0.1 || transformed_v2.w < 0.1)
            {
                //printf("clipping triangle %d\n", i);
                continue;
            }
            // Clipping


            if(transformed_v0.w != 0)
            {
                inv_w0 = 1.0f / transformed_v0.w;
                transformed_v0.x *= inv_w0;
                transformed_v0.y *= inv_w0;
                transformed_v0.z *= inv_w0;
            }
            
            if(transformed_v1.w != 0)
            {
                inv_w1 = 1.0f / transformed_v1.w;
                transformed_v1.x *= inv_w1;
                transformed_v1.y *= inv_w1;
                transformed_v1.z *= inv_w1;
            }
            
            if(transformed_v2.w != 0)
            {
                inv_w2 = 1.0f / transformed_v2.w;
                transformed_v2.x *= inv_w2;
                transformed_v2.y *= inv_w2;
                transformed_v2.z *= inv_w2;
            }

            v0.x = transformed_v0.x;
            v0.y = transformed_v0.y;
            v0.z = transformed_v0.z;

            v1.x = transformed_v1.x;
            v1.y = transformed_v1.y;
            v1.z = transformed_v1.z;

            v2.x = transformed_v2.x;
            v2.y = transformed_v2.y;
            v2.z = transformed_v2.z;
            ////// projection matrix ///////

            // @Performance This could be done before or after the viewport transform as long as the viewport doesnt flip Y
            f32 sign_area = orient_2d_v3(v0, v1, v2);
            if((render_flags & RenderFlags_Culling) && sign_area > 0)
            {
                game_state->culled_triangles++;
                continue;
            }

            ////// viewport transform //////
            v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
            v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
            v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
            v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;
            v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;
            v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;
            ////// viewport transform //////

            f32 min_x;
            f32 min_y;
            f32 max_x;
            f32 max_y;
            if(!triangle_screen_bounds(buffer, v0, v1, v2, &min_x, &max_x, &min_y, &max_y))
            {
                continue;
            }

            Vec3 new_vv0_color = vec3_scalar(vertex0.color, inv_w0);
            Vec3 new_vv1_color = vec3_scalar(vertex1.color, inv_w1);
            Vec3 new_vv2_color = vec3_scalar(vertex2.color, inv_w2);
            if((render_flags & RenderFlags_AffineUVInterpolation) == 0)
            {
                v0_uv = vec2_scalar(v0_uv, inv_w0);
                v1_uv = vec2_scalar(v1_uv, inv_w1);
                v2_uv = vec2_scalar(v2_uv, inv_w2);
            }

            Params params = 
            {
                view, persp,
                v0, v1, v2,
                new_vv0_color, new_vv1_color, new_vv2_color,
                v0_uv, v1_uv, v2_uv, texels, texels_width, texels_height,
                inv_w0, inv_w1, inv_w2,
                min_x, max_x, min_y, max_y
            };

            params.buffer = buffer;
            params.depth_buffer = depth_buffer;
            params.render_flags = render_flags;
            barycentric_with_edge_stepping(&params);


        }
    }
}
FontGlyph font_load_glyph(FT_Face face, char codepoint, FontInfo *info) {
    FontGlyph result = {0};
    // The index has nothing to do with the codepoint
    u32 glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index) 
    {
        FT_Error load_glyph_err = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT); 
        if (load_glyph_err) 
        {
            const char* err_str = FT_Error_String(load_glyph_err);
            printf("FT_Load_Glyph: %s\n", err_str);
        }

        FT_Error render_glyph_err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (render_glyph_err) 
        {
            const char* err_str = FT_Error_String(render_glyph_err);
            printf("FT_Load_Glyph: %s\n", err_str);
        }

		info->max_glyph_width = Max(info->max_glyph_width, face->glyph->bitmap.width);
		info->max_glyph_height = Max(info->max_glyph_height, face->glyph->bitmap.rows);

        result.bitmap.width = face->glyph->bitmap.width;
        result.bitmap.height = face->glyph->bitmap.rows;
        result.bitmap.pitch = face->glyph->bitmap.pitch;
        
        result.bitmap_top = face->glyph->bitmap_top;
        result.bitmap_left = face->glyph->bitmap_left;
        result.advance_x = face->glyph->advance.x;
        info->max_char_width = Max(info->max_char_width, result.advance_x);

        FT_Bitmap* bitmap = &face->glyph->bitmap;
        result.bitmap.buffer = (u8*)malloc(result.bitmap.pitch * result.bitmap.height);
        memcpy(result.bitmap.buffer, bitmap->buffer, result.bitmap.pitch * result.bitmap.height);
    }
    return result;
}


internal void
camera_handle_movement(Camera *camera, Game_Input *input, f32 dt)
{
    f32 speed = 1.0;
    if(input_is_key_pressed(input, Keys_W))
    {
        Vec3 forward = vec3_scalar(camera->forward, speed * dt);
        camera->position = vec3_add(camera->position, forward);
    }
    if(input_is_key_pressed(input, Keys_S))
    {
        Vec3 forward = vec3_scalar(camera->forward, speed * dt);
        camera->position = vec3_sub(camera->position, forward);
    }
    if(input_is_key_pressed(input, Keys_A))
    {
        Vec3 right = vec3_scalar(camera->right, speed * dt);
        camera->position = vec3_sub(camera->position, right);
    }
    if(input_is_key_pressed(input, Keys_D))
    {
        Vec3 right = vec3_scalar(camera->right, speed * dt);
        camera->position = vec3_add(camera->position, right);
        //Vec3 right = {.x = speed * dt, 0, 0};
        //camera->position = vec3_add(camera->position, right);
    }
    if(input_is_key_pressed(input, Keys_Space))
    {
        Vec3 up = {0, speed * dt, 0};
        camera->position = vec3_sub(camera->position, up);
    }
    if(input_is_key_pressed(input, Keys_Control))
    {
        Vec3 up = {0, speed * dt, 0};
        camera->position = vec3_add(camera->position, up);
    }
    {
        f32 speed = 0.001f;
        f32 dx = input->dx;
        f32 dy = input->dy;
        camera->pitch += dy * speed;
        camera->yaw += dx * speed; 
        f32 c_pitch = cos(camera->pitch);
        f32 c_yaw = cos(camera->yaw);
        f32 s_pitch = sin(camera->pitch);
        f32 s_yaw = sin(camera->yaw);
        Vec3 new_forward = {c_pitch * s_yaw, -s_pitch, c_pitch * c_yaw};
        //printf("%.2f, %.2f, %.2f\n", camera->forward.x, camera->forward.y, camera->forward.z);
        camera->forward = new_forward;
        camera->right = vec3_normalize(vec3_cross((Vec3){0, 1, 0}, new_forward));
        //printf("%.2f, %.2f, %.2f\n", camera->right.x, camera->right.y, camera->right.z);
    }
}

UPDATE_AND_RENDER(update_and_render)
{
    stbi_set_flip_vertically_on_load(1);
    Game_State *game_state = (Game_State*)game_memory->persistent_memory;
    font_info = game_state->font_info;
    timer_init();
    if(!game_memory->init)
    {
        printf("init\n");
        game_state->arena = arena_alloc_with_base(game_memory->persistent_memory_size - sizeof(Game_State), (u8*)game_memory->persistent_memory + sizeof(Game_State));
        g_transient_arena = arena_alloc_with_base(game_memory->transient_memory_size, (u8*)game_memory->transient_memory);
        Arena *arena = game_state->arena;

        {
            FT_Library library;
            FT_Error error = FT_Init_FreeType(&library);
            if (error)
            {
                const char* err_str = FT_Error_String(error);
                printf("FT_Init_FreeType: %s\n", err_str);
                exit(1);
            }

            const char *font_path = "C:\\Windows\\Fonts\\CascadiaMono.ttf";
            OS_FileReadResult font_file = os_file_read(arena, font_path);

            FT_Face face = {0};
            if(font_file.data)
            {
                FT_Open_Args args = {0};
                args.flags = FT_OPEN_MEMORY;
                args.memory_base = (u8*) font_file.data;
                args.memory_size = font_file.size;

                FT_Error opened_face = FT_Open_Face(library, &args, 0, &face);
                if (opened_face) 
                {
                    const char* err_str = FT_Error_String(opened_face);
                    printf("FT_Open_Face: %s\n", err_str);
                    exit(1);
                }

                FT_Error set_char_size_err = FT_Set_Char_Size(face, 4 * 64, 4 * 64, 300, 300);
                FT_Error set_char_size_erra = FT_Set_Pixel_Sizes(face, 0, 10);
                
                game_state->font_info.ascent = face->size->metrics.ascender >> 6;
                game_state->font_info.descent = - (face->size->metrics.descender >> 6); 
                game_state->font_info.line_height = face->size->metrics.height >> 6;

                {
                    for(u32 codepoint = '!'; codepoint <= '~'; codepoint++) {
                        game_state->font_info.font_table[(u32)codepoint] = font_load_glyph(face, (char)codepoint, &game_state->font_info);
                    }
                    game_state->font_info.font_table[(u32)(' ')] = font_load_glyph(face, (char)(' '), &game_state->font_info);
                }
            }
        }

        // models loading
        {
            OS_FileReadResult obj = os_file_read(arena, ".\\obj\\f22\\f22.obj");
            Obj_Model model = parse_obj(obj.data, obj.size);
            game_state->mesh_f22 = obj_build_mesh(game_state->arena, model);
            
            i32 x, y, n;
            u8 *data = stbi_load(".\\obj\\f22\\f22.png", &x, &y, &n, 0);

            assert(n == 4);

            u32 *texture_data = malloc(n * y * x);
            u32 *dest = texture_data;
            for(u32 i = 0; i < n * y * x; i+=n)
            {
                u8 r = data[i];
                u8 g = data[i + 1];
                u8 b = data[i + 2];
                u8 a = data[i + 3];

                u32 result = 0;
                result |= a << 24;
                result |= r << 16;
                result |= g << 8;
                result |= b << 0;
                *dest++ = result;
            }
            game_state->mesh_f22.texture_data = texture_data;
            game_state->mesh_f22.texture_width = x;
            game_state->mesh_f22.texture_height = y;
        }
        {
            OS_FileReadResult obj = os_file_read(arena, ".\\obj\\efa\\efa.obj");
            Obj_Model model = parse_obj(obj.data, obj.size);
            game_state->mesh_efa = obj_build_mesh(game_state->arena, model);
            i32 x, y, n;
            u8 *data = stbi_load(".\\obj\\efa\\efa.png", &x, &y, &n, 0);

            assert(n == 4);

            u32 *texture_data = malloc(n * y * x);
            u32 *dest = texture_data;
            for(u32 i = 0; i < n * y * x; i+=n)
            {
                u8 r = data[i];
                u8 g = data[i + 1];
                u8 b = data[i + 2];
                u8 a = data[i + 3];

                u32 result = 0;
                result |= a << 24;
                result |= r << 16;
                result |= g << 8;
                result |= b << 0;
                *dest++ = result;
            }
            game_state->mesh_efa.texture_data = texture_data;
            game_state->mesh_efa.texture_width = x;
            game_state->mesh_efa.texture_height = y;
        }
        {
            OS_FileReadResult obj = os_file_read(arena, ".\\obj\\f117\\f117.obj");
            Obj_Model model = parse_obj(obj.data, obj.size);
            game_state->mesh_f117 = obj_build_mesh(game_state->arena, model);
            i32 x, y, n;
            u8 *data = stbi_load(".\\obj\\f117\\f117.png", &x, &y, &n, 0);

            assert(n == 4);

            u32 *texture_data = malloc(n * y * x);
            u32 *dest = texture_data;
            for(u32 i = 0; i < n * y * x; i+=n)
            {
                u8 r = data[i];
                u8 g = data[i + 1];
                u8 b = data[i + 2];
                u8 a = data[i + 3];

                u32 result = 0;
                result |= a << 24;
                result |= r << 16;
                result |= g << 8;
                result |= b << 0;
                *dest++ = result;
            }
            game_state->mesh_f117.texture_data = texture_data;
            game_state->mesh_f117.texture_width = x;
            game_state->mesh_f117.texture_height = y;
        }


        game_state->camera.position = (Vec3) {1.79, -1.53, -2.58};
        game_state->camera.pitch = -0.39;
        game_state->camera.yaw = -0.46;
        game_state->camera.forward = (Vec3) {0, 0, 1};

        game_state->example = 14;
        game_state->example_count = 14;
        game_state->example12_lighting_mode = Example12Lighting_Flat;



        game_memory->init = 1;
    }

    LONGLONG frame_time_now = timer_get_os_time();
    u32 steam_chat_background_color = 0xff1e2025;
    clear_screen(buffer, steam_chat_background_color);
    clear_depth_buffer(depth_buffer);
    
    // My idea here is the following. I'm trying to understand how culling works so I know that if i have a triangle given in CCW
    // the same exact triangle when looking from the back its going to be seen as CW. So this depends on the view itself.
    // So I'm planning to make two examples: 
    //
    // The first example is going to have just two triangles, with Y+ UP DOWN, NO view and projection matrices, just a viewport transform and CCW means front face.
    // One triangle is going to be CCW and the other one CW. Because the triangles have 0.5 as z that means that I want to reject all
    // triangles where its normal is > 0 which mean it would point opposite to the camera (because the camera is through Z = 1)
    // (The last setence was just an assumption because I'm not dictating the Z range as I dont use view or projection matrices. So its weird for now!)
    //
    // The second example is going to use a perspective projection and I will try with the regular projection I'm using `mat4_make_perspective`
    // and some other projection where Z is inverse. The expectation here is that using the regular projection will work the same, and using
    // the inverse Z projection works opposite.


    // When I used opengl and i didnt want to setup a view or a projetion matrix i would just
    // define the vertices in NDC space of OpenGL and it would render a triangle. Its the same thing
    // that happens in learnopengl at the start of the tutorial. 
    // My viewport transforms maps from -1 to 1 on x and y that means that my vertices should be between
    // [-1, 1] for both x and y.

    if(input_is_key_just_pressed(input, Keys_Arrow_Left) && game_state->example > 1)
    {
        game_state->example--;
    }
    if(input_is_key_just_pressed(input, Keys_Arrow_Right) && game_state->example < game_state->example_count)
    {
        game_state->example++;
    }

    if(game_state->example == 1)
    {
        // Example 1
        // here are two triangles one is ccw and the other cw
        // This is CW and is to the left
        Triangle cw_tri = {
            .v0 = (Vec3) {-0.5, -0.25, 0.5f},
            .v1 = (Vec3) {0.0, -0.25, 0.5f},
            .v2 = (Vec3) {-0.25, 0.25, 0.5f},
            .text = str8_literal("CW"),
        };
        f32 sign_cw = orient_2d_v3(cw_tri.v0, cw_tri.v1, cw_tri.v2);
        cw_tri.sign_area = sign_cw;
        
        // This is CCW and is to the right
        Triangle ccw_tri = {
            .v0 = (Vec3) { 0.0f, 0.0f, 0.5f },
            .v1 = (Vec3) { 0.25f, 0.50f, 0.5f },
            .v2 = (Vec3) { 0.5f,  0.0f, 0.5f },
            .text = str8_literal("CCW"),
        };
        f32 sign_ccw = orient_2d_v3(ccw_tri.v0, ccw_tri.v1, ccw_tri.v2);
        ccw_tri.sign_area = sign_ccw;
        Triangle triangles[2] = {cw_tri, ccw_tri};
        Mat4 view = mat4_identity();
        Mat4 persp = mat4_identity();
        //provisionary_block(buffer, depth_buffer, triangles, 2, view, persp, RenderFlags_TwoSidedRasterization);
        Vertex vertices[6] = {
            (Vertex) {.position = cw_tri.v0  , .color = {255, 0, 0}},
            (Vertex) {.position = cw_tri.v1  , .color = {0, 255, 0}},
            (Vertex) {.position = cw_tri.v2  , .color = {0, 0, 255}},
            (Vertex) {.position = ccw_tri.v0 , .color = {255, 0, 0}},
            (Vertex) {.position = ccw_tri.v1 , .color = {0, 255, 0}},
            (Vertex) {.position = ccw_tri.v2 , .color = {0, 0, 255}},
        };
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 6, 0, 0, 0, 0, 0, view, persp, RenderFlags_TwoSidedRasterization);
    }
    if(game_state->example == 2)
    {
        // Example 2
        // Same two triangles as before view almost the same perspective matrix but z ranges from [-1, 0] instead of
        // [0, 1] like previously!
        // This is CW and is to the left
        Triangle cw_tri = {
            .v0 = (Vec3) { -0.5, -0.25, 0.5f},
            .v1 = (Vec3) { 0.0, -0.25, 0.5f},
            .v2 = (Vec3) { -0.25, 0.25, 0.5f},
            .text = str8_literal("CW"),
        };
        f32 sign_cw = orient_2d_v3(cw_tri.v0, cw_tri.v1, cw_tri.v2);
        cw_tri.sign_area = sign_cw;
        
        // This is CCW and is to the right
        Triangle ccw_tri = {
            .v0 = (Vec3) { 0.0f, 0.0f, 0.5f },
            .v1 = (Vec3) { 0.25f, 0.50f, 0.5f },
            .v2 = (Vec3) { 0.5f,  0.0f, 0.5f },
            .text = str8_literal("CCW"),
        };
        f32 sign_ccw = orient_2d_v3(ccw_tri.v0, ccw_tri.v1, ccw_tri.v2);
        ccw_tri.sign_area = sign_ccw;
        Triangle triangles[2] = {cw_tri, ccw_tri};
        Mat4 view = mat4_identity();

        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        Vertex vertices[6] = {
            (Vertex) {.position = cw_tri.v0  , .color = {255, 0, 0}},
            (Vertex) {.position = cw_tri.v1  , .color = {0, 255, 0}},
            (Vertex) {.position = cw_tri.v2  , .color = {0, 0, 255}},
            (Vertex) {.position = ccw_tri.v0 , .color = {255, 0, 0}},
            (Vertex) {.position = ccw_tri.v1 , .color = {0, 255, 0}},
            (Vertex) {.position = ccw_tri.v2 , .color = {0, 0, 255}},
        };
        //provisionary_block(buffer, depth_buffer, triangles, 2, view, persp, RenderFlags_TwoSidedRasterization);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 6, 0, 0, 0, 0, 0, view, persp, RenderFlags_TwoSidedRasterization);
    }
    if(game_state->example == 3)
    {
        // Example 3
        // This could be like example 1 with a camera... seems good right?
        Triangle cw_tri = {
            .v0 = (Vec3) { -0.5, -0.25, 5.5f},
            .v1 = (Vec3) { 0.0, -0.25, 5.5f},
            .v2 = (Vec3) { -0.25, 0.25, 5.5f},
            .text = str8_literal("CW"),
        };
        f32 sign_cw = orient_2d_v3(cw_tri.v0, cw_tri.v1, cw_tri.v2);
        cw_tri.sign_area = sign_cw;
        
        // This is CCW and is to the right
        Triangle ccw_tri = {
            .v0 = (Vec3) { 0.0f, 0.0f, 5.5f },
            .v1 = (Vec3) { 0.25f, 0.50f, 5.5f },
            .v2 = (Vec3) { 0.5f,  0.0f, 5.5f },
            .text = str8_literal("CCW"),
        };
        f32 sign_ccw = orient_2d_v3(ccw_tri.v0, ccw_tri.v1, ccw_tri.v2);
        ccw_tri.sign_area = sign_ccw;
        Triangle triangles[2] = {cw_tri, ccw_tri};
        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        //provisionary_block(buffer, depth_buffer, triangles + 1, 1, view, persp, RenderFlags_Culling);
        Vertex vertices[6] = {
            (Vertex) {.position = cw_tri.v0  , .color = {255, 0, 0}},
            (Vertex) {.position = cw_tri.v1  , .color = {0, 255, 0}},
            (Vertex) {.position = cw_tri.v2  , .color = {0, 0, 255}},
            (Vertex) {.position = ccw_tri.v0 , .color = {255, 0, 0}},
            (Vertex) {.position = ccw_tri.v1 , .color = {0, 255, 0}},
            (Vertex) {.position = ccw_tri.v2 , .color = {0, 0, 255}},
        };
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 6, 0, 0, 0, 0, 0, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 4)
    {
        // Example 4
        // Both CCW
        Triangle ccw_tri = {
            .v0 = (Vec3) { 0.0f, 0.0f, 5.5f },
            .v1 = (Vec3) { 0.0f, 0.50f, 5.5f },
            .v2 = (Vec3) { 0.5f,  0.0f, 5.5f },
            .text = str8_literal("CCW"),
        };
        f32 sign_ccw = orient_2d_v3(ccw_tri.v0, ccw_tri.v1, ccw_tri.v2);
        ccw_tri.sign_area = sign_ccw;

        Triangle ccw_tri2 = {
            .v0 = (Vec3) { 0.5f,  0.0f, 5.5f },
            .v1 = (Vec3) { 0.0f, 0.5f, 5.5f },
            .v2 = (Vec3) { 0.5f, 0.5f, 5.5f },
            .text = str8_literal("CCW"),
        };
        f32 sign_ccw2 = orient_2d_v3(ccw_tri2.v0, ccw_tri2.v1, ccw_tri2.v2);
        ccw_tri2.sign_area = sign_ccw2;
        Triangle triangles[2] = {ccw_tri, ccw_tri2};
        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        Vertex vertices[6] = {
            (Vertex) {.position = ccw_tri.v0  , .color = {255, 0, 0}},
            (Vertex) {.position = ccw_tri.v1  , .color = {0, 255, 0}},
            (Vertex) {.position = ccw_tri.v2  , .color = {0, 0, 255}},
            (Vertex) {.position = ccw_tri2.v0 , .color = {255, 0, 0}},
            (Vertex) {.position = ccw_tri2.v1 , .color = {0, 255, 0}},
            (Vertex) {.position = ccw_tri2.v2 , .color = {0, 0, 255}},
        };
        //provisionary_block(buffer, depth_buffer, triangles, 2, view, persp, RenderFlags_Culling);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 6, 0, 0, 0, 0, 0, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 5)
    {
        // same as example 4 but with indices
        // Both CCW
        Vertex vertices[4] = {
            (Vertex){ .position = { 0.0f, 0.0f, 5.5f }, .color = {0, 255, 0}},
            (Vertex){ .position = { 0.0f, 0.50f, 5.5f }, .color = {255, 0, 0}},
            (Vertex){ .position = { 0.5f,  0.0f, 5.5f }, .color = {255, 255, 255}},
            (Vertex){ .position = { 0.5f, 0.5f, 5.5f }, .color = {0, 0, 255}},
        };
        u32 indices[6] = {0, 1, 2, 2, 1, 3};

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 4, indices, 6, 0, 0, 0, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 6)
    {
        // same as example 5 but now with texturing
        u32 texels[4] = {
            0xFF000000, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFF000000
        };
        // Both CCW
        #if 0
        Vertex vertices[4] = {
            (Vertex){ .position = { 0.0f, 0.0f, 5.5f },  .color = {0, 255, 0}},
            (Vertex){ .position = { 0.0f, 0.50f, 5.5f }, .color = {255, 0, 0}},
            (Vertex){ .position = { 0.5f,  0.0f, 5.5f }, .color = {255, 255, 255}},
            (Vertex){ .position = { 0.5f, 0.5f, 5.5f },  .color = {0, 0, 255}},
        };
        #else
        Vertex vertices[4] = {
            (Vertex){ .position = { 0.0f, 0.0f, 2.5f },  .color = {255, 255, 255}, .uv = {0, 0}},
            (Vertex){ .position = { 0.0f, 0.50f, 2.5f }, .color = {255, 255, 255}, .uv = {0, 1}},
            (Vertex){ .position = { 0.5f,  0.0f, 2.5f }, .color = {255, 255, 255}, .uv = {1, 0}},
            (Vertex){ .position = { 0.5f, 0.5f, 2.5f },  .color = {255, 255, 255}, .uv = {1, 1}},
        };
        #endif

        u32 indices[6] = {0, 1, 2, 2, 1, 3};

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 4, indices, 6, texels, 2, 2, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 7)
    {
        // Two quads side by side, each with its own 0..1 UVs, so the checker restarts on the seam.
        u32 texels[4] = {
            0xFF000000, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFF000000
        };
        Vertex vertices[8] = {
            (Vertex){ .position = { 0.0f, 0.0f, 2.5f },  .color = {255, 128, 128}, .uv = {0, 0}},
            (Vertex){ .position = { 0.0f, 0.50f, 2.5f }, .color = {255, 128, 128}, .uv = {0, 1}},
            (Vertex){ .position = { 0.5f, 0.0f, 2.5f },  .color = {255, 128, 128}, .uv = {1, 0}},
            (Vertex){ .position = { 0.5f, 0.5f, 2.5f },  .color = {255, 128, 128}, .uv = {1, 1}},
            (Vertex){ .position = { 0.5f, 0.0f, 2.5f },  .color = {128, 192, 255}, .uv = {0, 0}},
            (Vertex){ .position = { 0.5f, 0.5f, 2.5f },  .color = {128, 192, 255}, .uv = {0, 1}},
            (Vertex){ .position = { 1.0f, 0.0f, 2.5f },  .color = {128, 192, 255}, .uv = {1, 0}},
            (Vertex){ .position = { 1.0f, 0.5f, 2.5f },  .color = {128, 192, 255}, .uv = {1, 1}},
        };
        u32 indices[12] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 8, indices, 12, texels, 2, 2, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 8)
    {
        // Same as example 7, but rotate the whole strip to inspect perspective-correct texture interpolation.
        u32 texels[4] = {
            0xFF000000, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFF000000
        };
        Vertex vertices[8] = {
            (Vertex){ .position = { 0.0f, 0.0f, 2.5f },  .color = {255, 128, 128}, .uv = {0, 0}},
            (Vertex){ .position = { 0.0f, 0.50f, 2.5f }, .color = {255, 128, 128}, .uv = {0, 1}},
            (Vertex){ .position = { 0.5f, 0.0f, 2.5f },  .color = {255, 128, 128}, .uv = {1, 0}},
            (Vertex){ .position = { 0.5f, 0.5f, 2.5f },  .color = {255, 128, 128}, .uv = {1, 1}},
            (Vertex){ .position = { 0.5f, 0.0f, 2.5f },  .color = {128, 192, 255}, .uv = {0, 0}},
            (Vertex){ .position = { 0.5f, 0.5f, 2.5f },  .color = {128, 192, 255}, .uv = {0, 1}},
            (Vertex){ .position = { 1.0f, 0.0f, 2.5f },  .color = {128, 192, 255}, .uv = {1, 0}},
            (Vertex){ .position = { 1.0f, 0.5f, 2.5f },  .color = {128, 192, 255}, .uv = {1, 1}},
        };
        u32 indices[12] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};

        static f32 example_8_angle = 0.0f;
        example_8_angle += dt;

        Vec3 center = {0.5f, 0.25f, 2.5f};
        rotate_vertices_about_center(vertices, 8, center, example_8_angle, example_8_angle * 0.5f);

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, 8, indices, 12, texels, 2, 2, view, persp, RenderFlags_TwoSidedRasterization);
    }
    if(game_state->example == 9)
    {
        // Left strip uses perspective-correct UVs, right strip uses affine UVs.
        u32 texels[4] = {
            0xFF000000, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFF000000
        };
        u32 indices[12] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
        Vertex vertices_perspective[8] = {
            (Vertex){ .position = {-1.15f, 0.0f, 3.0f }, .color = {255, 128, 128}, .uv = {0, 0}},
            (Vertex){ .position = {-1.15f, 0.5f, 3.0f }, .color = {255, 128, 128}, .uv = {0, 1}},
            (Vertex){ .position = {-0.65f, 0.0f, 3.0f }, .color = {255, 128, 128}, .uv = {1, 0}},
            (Vertex){ .position = {-0.65f, 0.5f, 3.0f }, .color = {255, 128, 128}, .uv = {1, 1}},
            (Vertex){ .position = {-0.65f, 0.0f, 3.0f }, .color = {128, 192, 255}, .uv = {0, 0}},
            (Vertex){ .position = {-0.65f, 0.5f, 3.0f }, .color = {128, 192, 255}, .uv = {0, 1}},
            (Vertex){ .position = {-0.15f, 0.0f, 3.0f }, .color = {128, 192, 255}, .uv = {1, 0}},
            (Vertex){ .position = {-0.15f, 0.5f, 3.0f }, .color = {128, 192, 255}, .uv = {1, 1}},
        };
        Vertex vertices_affine[8] = {
            (Vertex){ .position = {0.15f, 0.0f, 3.0f }, .color = {255, 128, 128}, .uv = {0, 0}},
            (Vertex){ .position = {0.15f, 0.5f, 3.0f }, .color = {255, 128, 128}, .uv = {0, 1}},
            (Vertex){ .position = {0.65f, 0.0f, 3.0f }, .color = {255, 128, 128}, .uv = {1, 0}},
            (Vertex){ .position = {0.65f, 0.5f, 3.0f }, .color = {255, 128, 128}, .uv = {1, 1}},
            (Vertex){ .position = {0.65f, 0.0f, 3.0f }, .color = {128, 192, 255}, .uv = {0, 0}},
            (Vertex){ .position = {0.65f, 0.5f, 3.0f }, .color = {128, 192, 255}, .uv = {0, 1}},
            (Vertex){ .position = {1.15f, 0.0f, 3.0f }, .color = {128, 192, 255}, .uv = {1, 0}},
            (Vertex){ .position = {1.15f, 0.5f, 3.0f }, .color = {128, 192, 255}, .uv = {1, 1}},
        };

        static f32 example_9_time = 0.0f;
        example_9_time += dt;
        f32 angle_y = 0.85f * sinf(example_9_time);
        f32 angle_x = 0.45f * cosf(example_9_time * 0.7f);
        rotate_vertices_about_center(vertices_perspective, 8, (Vec3){-0.65f, 0.25f, 3.0f}, angle_y, angle_x);
        rotate_vertices_about_center(vertices_affine, 8, (Vec3){0.65f, 0.25f, 3.0f}, angle_y, angle_x);

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices_perspective, 8, indices, 12, texels, 2, 2, view, persp, RenderFlags_Culling);
        provisionary_block2(game_state, buffer, depth_buffer, vertices_affine, 8, indices, 12, texels, 2, 2, view, persp, RenderFlags_Culling | RenderFlags_AffineUVInterpolation);
    }
    if(game_state->example == 10)
    {
        // Tiled floor built from many quads. Each tile reuses the same checker UVs,
        // while the tint repeats every few tiles to make the pattern easier to read.
        u32 texels[4] = {
            0xFF141414, 0xFFF0F0F0,
            0xFFF0F0F0, 0xFF141414
        };
        enum
        {
            floor_columns = 12,
            floor_rows = 18,
            floor_quad_count = floor_columns * floor_rows,
            floor_vertex_count = floor_quad_count * 4,
            floor_index_count = floor_quad_count * 6,
        };
        Vertex vertices[floor_vertex_count];
        u32 indices[floor_index_count];
        Vec3 tint_palette[4] = {
            {255.0f, 225.0f, 225.0f},
            {225.0f, 255.0f, 235.0f},
            {225.0f, 235.0f, 255.0f},
            {255.0f, 245.0f, 210.0f},
        };
        f32 tile_width = 1.0f;
        f32 tile_depth = 1.0f;
        f32 floor_y = 1.2f;
        f32 start_x = -((f32)floor_columns * tile_width) * 0.5f;
        f32 start_z = 2.0f;
        u32 vertex_cursor = 0;
        u32 index_cursor = 0;
        for(u32 row = 0; row < floor_rows; row++)
        {
            for(u32 column = 0; column < floor_columns; column++)
            {
                f32 x0 = start_x + (f32)column * tile_width;
                f32 x1 = x0 + tile_width;
                f32 z0 = start_z + (f32)row * tile_depth;
                f32 z1 = z0 + tile_depth;
                Vec3 tint = tint_palette[(row + column) % ArrayCount(tint_palette)];
                tint = (Vec3) {.x = 255, .y = 255, .z = 255};

                vertices[vertex_cursor + 0] = (Vertex){ .position = {x0, floor_y, z0}, .color = tint, .uv = {0, 0} };
                vertices[vertex_cursor + 1] = (Vertex){ .position = {x0, floor_y, z1}, .color = tint, .uv = {0, 1} };
                vertices[vertex_cursor + 2] = (Vertex){ .position = {x1, floor_y, z0}, .color = tint, .uv = {1, 0} };
                vertices[vertex_cursor + 3] = (Vertex){ .position = {x1, floor_y, z1}, .color = tint, .uv = {1, 1} };

                indices[index_cursor + 0] = vertex_cursor + 0;
                indices[index_cursor + 1] = vertex_cursor + 2;
                indices[index_cursor + 2] = vertex_cursor + 1;
                indices[index_cursor + 3] = vertex_cursor + 2;
                indices[index_cursor + 4] = vertex_cursor + 3;
                indices[index_cursor + 5] = vertex_cursor + 1;

                vertex_cursor += 4;
                index_cursor += 6;
            }
        }

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592f / 3.0f; // 60 deg
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, floor_vertex_count, indices, floor_index_count, texels, 2, 2, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 11)
    {
        //clear_screen(buffer, 0xFF8D86D9);
        enum
        {
            terrain_columns = 36,
            terrain_rows = 80,
            max_scene_vertices = terrain_columns * terrain_rows * 2 * 3,
        };

        Vertex vertices[max_scene_vertices];
        Vec3 light_dir = vec3_normalize((Vec3){-0.60f, -1.0f, 0.30f});
        f32 ambient = 0.48f;
        f32 diffuse = 0.60f;
        Example12LightingMode lighting_mode = (Example12LightingMode)game_state->example12_lighting_mode;
        u32 vertex_count = build_ps1_terrain_patch(vertices, max_scene_vertices,
                                                   terrain_columns, terrain_rows, 0.95f, 0.95f,
                                                   -7.6f, 4.2f,
                                                   lighting_mode,
                                                   light_dir, ambient, diffuse);

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592f / 3.0f;
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 60.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, vertex_count, 0, 0, 0, 0, 0, view, persp, RenderFlags_Culling);
    }
    if(game_state->example == 12)
    {
        clear_screen(buffer, 0xFF17111B);

        enum
        {
            shock_segment_count = 14,
            spike_count = 18,
            smoke_count = 7,
            max_scene_vertices = 600,
        };

        static f32 example_13_time = 0.0f;
        if(input_is_key_just_pressed(input, Keys_R))
        {
            example_13_time = 0.0f;
        }
        example_13_time += dt;

        f32 loop_duration = 2.8f;
        f32 t = fmodf(example_13_time, loop_duration);
        f32 burst = 1.0f - smoothstep_f32(0.18f, 1.05f, t);
        f32 flash = 1.0f - smoothstep_f32(0.02f, 0.32f, t);
        f32 smoke = ease_out_cubic(saturate_f32((t - 0.08f) / 1.45f));

        Vertex vertices[max_scene_vertices];
        u32 vertex_count = 0;
        Vec3 light_dir = vec3_normalize((Vec3){-0.35f, -1.0f, 0.25f});
        Vec3 center = {0.0f, 1.52f, 8.0f};
        f32 ground_y = 1.95f;

        push_flat_quad(vertices, max_scene_vertices, &vertex_count,
                       (Vec3){-7.0f, ground_y, 4.0f},
                       (Vec3){-7.0f, ground_y, 15.0f},
                       (Vec3){ 7.0f, ground_y, 15.0f},
                       (Vec3){ 7.0f, ground_y, 4.0f},
                       (Vec3){54.0f, 43.0f, 36.0f},
                       light_dir, 0.45f, 0.30f);

        if(t < 0.95f)
        {
            f32 ring_outer = 0.35f + 3.6f * ease_out_cubic(saturate_f32(t / 0.62f));
            f32 ring_inner = Max(0.0f, ring_outer - 0.22f);
            Vec3 ring_inner_color = color_scale((Vec3){255.0f, 214.0f, 96.0f}, 0.55f + flash * 0.55f);
            Vec3 ring_outer_color = color_scale((Vec3){196.0f, 82.0f, 30.0f}, 0.45f + burst * 0.35f);
            f32 ring_y = ground_y - 0.02f;

            for(u32 segment_index = 0; segment_index < shock_segment_count; segment_index++)
            {
                f32 angle0 = ((f32)segment_index / (f32)shock_segment_count) * 2.0f * 3.141592f;
                f32 angle1 = ((f32)(segment_index + 1) / (f32)shock_segment_count) * 2.0f * 3.141592f;
                Vec3 inner0 = {center.x + cosf(angle0) * ring_inner, ring_y, center.z + sinf(angle0) * ring_inner};
                Vec3 inner1 = {center.x + cosf(angle1) * ring_inner, ring_y, center.z + sinf(angle1) * ring_inner};
                Vec3 outer0 = {center.x + cosf(angle0) * ring_outer, ring_y, center.z + sinf(angle0) * ring_outer};
                Vec3 outer1 = {center.x + cosf(angle1) * ring_outer, ring_y, center.z + sinf(angle1) * ring_outer};

                push_triangle(vertices, max_scene_vertices, &vertex_count, inner0, outer0, inner1, ring_inner_color, ring_outer_color, ring_inner_color);
                push_triangle(vertices, max_scene_vertices, &vertex_count, outer0, outer1, inner1, ring_outer_color, ring_outer_color, ring_inner_color);
            }
        }

        {
            Vec3 flash_color = color_scale((Vec3){255.0f, 238.0f, 176.0f}, 0.55f + flash * 0.75f);
            Vec3 ember_color = color_scale((Vec3){255.0f, 118.0f, 32.0f}, 0.45f + burst * 0.45f);
            f32 core_radius = 0.16f + flash * 0.42f;
            push_flat_octahedron(vertices, max_scene_vertices, &vertex_count,
                                 (Vec3){center.x, center.y - burst * 0.18f, center.z},
                                 (Vec3){core_radius * 0.95f, core_radius * 1.18f, core_radius * 0.95f},
                                 flash_color,
                                 light_dir, 0.34f, 0.78f);
            push_flat_octahedron(vertices, max_scene_vertices, &vertex_count,
                                 (Vec3){center.x, center.y - burst * 0.10f, center.z},
                                 (Vec3){core_radius * 1.45f, core_radius * 0.85f, core_radius * 1.45f},
                                 ember_color,
                                 light_dir, 0.34f, 0.78f);
        }

        for(u32 spike_index = 0; spike_index < spike_count; spike_index++)
        {
            f32 angle = hash01(spike_index * 11 + 3) * 2.0f * 3.141592f;
            f32 spread = 0.75f + hash01(spike_index * 13 + 9) * 0.35f;
            f32 lift = -0.22f - hash01(spike_index * 17 + 1) * 0.60f;
            Vec3 dir = vec3_normalize((Vec3){cosf(angle) * spread, lift, sinf(angle) * spread});
            Vec3 right = vec3_cross(dir, (Vec3){0.0f, 1.0f, 0.0f});
            if(vec3_magnitude_squared(right) <= EPSILON)
            {
                right = (Vec3){1.0f, 0.0f, 0.0f};
            }
            right = vec3_normalize(right);

            f32 spike_length = (0.75f + burst * 2.2f) * (0.70f + hash01(spike_index * 19 + 2) * 0.75f);
            f32 spike_width = (0.07f + burst * 0.16f) * (0.80f + hash01(spike_index * 23 + 7) * 0.65f);
            Vec3 tip = vec3_add(center, color_scale(dir, spike_length));
            Vec3 p0 = vec3_add(center, color_scale(right, spike_width));
            Vec3 p1 = vec3_sub(center, color_scale(right, spike_width));

            Vec3 base_color = color_scale((Vec3){255.0f, 221.0f, 115.0f}, 0.55f + flash * 0.65f);
            Vec3 tip_color = color_scale((Vec3){235.0f, 75.0f, 18.0f}, 0.48f + burst * 0.55f);
            push_triangle(vertices, max_scene_vertices, &vertex_count, p0, p1, tip, base_color, base_color, tip_color);
        }

        for(u32 smoke_index = 0; smoke_index < smoke_count; smoke_index++)
        {
            f32 angle = hash01(smoke_index * 31 + 5) * 2.0f * 3.141592f;
            f32 drift = smoke * (0.35f + hash01(smoke_index * 29 + 8) * 1.4f);
            f32 rise = smoke * (0.55f + hash01(smoke_index * 37 + 2) * 1.35f);
            f32 scale = 0.22f + smoke * 0.34f;

            Vec3 puff_center =
            {
                center.x + cosf(angle) * drift,
                center.y - 0.18f - rise,
                center.z + sinf(angle) * drift,
            };
            Vec3 puff_color = vec3_lerp((Vec3){168.0f, 110.0f, 68.0f}, (Vec3){74.0f, 74.0f, 79.0f}, smoke);
            push_flat_octahedron(vertices, max_scene_vertices, &vertex_count,
                                 puff_center,
                                 (Vec3){scale, scale * 0.85f, scale},
                                 puff_color,
                                 light_dir, 0.46f, 0.42f);
        }

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592f / 3.0f;
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 40.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, vertices, vertex_count, 0, 0, 0, 0, 0, view, persp, RenderFlags_TwoSidedRasterization);
    }
    if(game_state->example == 13)
    {
        Vec3 colors[] = {
            red,
            green,
            blue ,
            white,
            yellow,
        };

        local_persist b32 generated = 0;
        local_persist u32 color_indexer = 0;
        if(!generated)
        {
            Vec3 light_dir = vec3_normalize((Vec3){-0.60f, -1.0f, 0.30f});
            f32 ambient = 0.48f;
            f32 diffuse = 0.60f;
            game_state->terrain_vertices_count = 0;
            game_state->terrain_indices_count = 0;
            game_state->terrain_vertices = malloc(sizeof(Vertex) * 40000);
            game_state->terrain_indices = malloc(sizeof(u32) * 40000);
            Vec3 start = {0};
            for(u32 row = 0; row < 40; row++)
            {
                for(u32 col = 0; col < 40; col++)
                {
                    f32 noise = stb_perlin_noise3(col, row, 0, 0, 0, 0);
                    Vec3 offset = vec3_add(start, (Vec3) {.x = col * 0.5f, .y = 0, .z = row * 0.5f});
                    f32 x0 = offset.x;
                    f32 x1 = offset.x + 0.5f;
                    f32 z0 = offset.z;
                    f32 z1 = offset.z + 0.5f;
                    Vec3 p00 = {x0, ps1_grass_height(x0, z0), z0};
                    Vec3 p10 = {x1, ps1_grass_height(x1, z0), z0};
                    Vec3 p01 = {x0, ps1_grass_height(x0, z1), z1};
                    Vec3 p11 = {x1, ps1_grass_height(x1, z1), z1};
                    p00.y = stb_perlin_noise3(x0, z0, 0, 0, 0, 0);
                    p10.y = stb_perlin_noise3(x1, z0, 0, 0, 0, 0);
                    p01.y = stb_perlin_noise3(x0, z1, 0, 0, 0, 0);
                    p11.y = stb_perlin_noise3(x1, z1, 0, 0, 0, 0);

                    Vec3 tri0_center = color_scale(vec3_add(vec3_add(p00, p10), p01), 1.0f / 3.0f);
                    Vec3 tri1_center = color_scale(vec3_add(vec3_add(p10, p11), p01), 1.0f / 3.0f);
                    Vec3 tri0_base = ps1_grass_base_color(tri0_center.x, tri0_center.z, tri0_center.y);
                    Vec3 tri1_base = ps1_grass_base_color(tri1_center.x, tri1_center.z, tri1_center.y);
                    Vec3 tri0_normal = vec3_normalize(vec3_cross(vec3_sub(p10, p00), vec3_sub(p01, p00)));
                    Vec3 tri1_normal = vec3_normalize(vec3_cross(vec3_sub(p11, p10), vec3_sub(p01, p10)));
                    Vec3 tri0_color = shade_directional(tri0_base, tri0_normal, light_dir, ambient, diffuse);
                    Vec3 tri1_color = shade_directional(tri1_base, tri1_normal, light_dir, ambient, diffuse);
                    Vec3 shared_color = color_scale(vec3_add(tri0_color, tri1_color), 0.5f);
                    color_indexer = color_indexer++ % ArrayCount(colors);
                    game_state->terrain_vertices[game_state->terrain_vertices_count + 0] = (Vertex){ .position = vec3_add(offset, (Vec3) { 0.0f, p01.y, 0.5f }),  .color = shared_color};
                    game_state->terrain_vertices[game_state->terrain_vertices_count + 1] = (Vertex){ .position = vec3_add(offset, (Vec3) { 0.0f, p00.y, 0.0f }),  .color = tri0_color};
                    game_state->terrain_vertices[game_state->terrain_vertices_count + 2] = (Vertex){ .position = vec3_add(offset, (Vec3) { 0.5f, p10.y, 0.0f }),  .color = shared_color};
                    game_state->terrain_vertices[game_state->terrain_vertices_count + 3] = (Vertex){ .position = vec3_add(offset, (Vec3) { 0.5f, p11.y, 0.5f }),  .color = tri1_color};

                    game_state->terrain_indices[game_state->terrain_indices_count + 0] = game_state->terrain_vertices_count + 0;
                    game_state->terrain_indices[game_state->terrain_indices_count + 1] = game_state->terrain_vertices_count + 1;
                    game_state->terrain_indices[game_state->terrain_indices_count + 2] = game_state->terrain_vertices_count + 2;
                    game_state->terrain_indices[game_state->terrain_indices_count + 3] = game_state->terrain_vertices_count + 0;
                    game_state->terrain_indices[game_state->terrain_indices_count + 4] = game_state->terrain_vertices_count + 2;
                    game_state->terrain_indices[game_state->terrain_indices_count + 5] = game_state->terrain_vertices_count + 3;
                    game_state->terrain_vertices_count += 4;
                    game_state->terrain_indices_count += 6;
                }
            }
            generated = 1;
        }

        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592f / 3.0f;
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 40.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block2(game_state, buffer, depth_buffer, game_state->terrain_vertices, game_state->terrain_vertices_count, game_state->terrain_indices, game_state->terrain_indices_count, 0, 0, 0, view, persp, RenderFlags_TwoSidedRasterization);
    }
    if(game_state->example == 14)
    {
        
        camera_handle_movement(&game_state->camera, input, dt);

        Mat4 view = mat4_look_at(game_state->camera.position, vec3_add(game_state->camera.position, game_state->camera.forward), (Vec3) {0.0f, 1.0f, 0.0f});
        f32 fov = 3.141592f / 3.0f;
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 0.1f;
        f32 zfar = 40.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        Mesh *mesh = &game_state->mesh_f117;
        mesh = &game_state->mesh_f117;
        mesh = &game_state->mesh_efa;
        mesh = &game_state->mesh_f22;

        //printf("Rendering vertices: %d , indices: %d\n", mesh->vertices_count, mesh->indices_count);
        provisionary_block2(game_state, buffer, depth_buffer,
            mesh->vertices, mesh->vertices_count, mesh->indices, mesh->indices_count, mesh->texture_data, mesh->texture_width, mesh->texture_height,
            view, persp, 
            0);
    }

    char buf[200];
    snprintf(buf, 200, "Running example: %d", game_state->example);
    draw_text(buffer, 4, 20, buf);
    {
        LONGLONG frame_time = timer_get_os_time() - frame_time_now;
        char buf[200];
        snprintf(buf, 200, "frame time: %.2fms", timer_os_time_to_ms(frame_time));
        draw_text(buffer, 4, 35, buf);
    }

    {
        // mouse
        f32 mouse_x = input->curr_mouse_state.x;
        f32 mouse_y = input->curr_mouse_state.y;
        char buf[200];
        snprintf(buf, 200, "Mouse position: (%.2f %.2f)", mouse_x, mouse_y);
        draw_text(buffer, 4, 50, buf);
    }
    {
        // camera
        Camera camera = game_state->camera;
        char buf[200];
        snprintf(buf, 200, "Camera position: (%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z);
        draw_text(buffer, 4, 65, buf);
    }
    {
        char buf[200];
        snprintf(buf, 200, "mouse deltas: (%.2f, %.2f)", input->dx, input->dy);
        draw_text(buffer, 4, 80, buf);
    }
    {
        char buf[200];
        snprintf(buf, 200, "camera pitch and yaw: (%.2f, %.2f)", game_state->camera.pitch, game_state->camera.yaw);
        draw_text(buffer, 4, 95, buf);
    }
    if(game_state->example == 9)
    {
        draw_text(buffer, 4, 110, "Left: perspective correct | Right: affine");
    }
    if(game_state->example == 10)
    {
        draw_text(buffer, 4, 110, "Tiled checker floor with repeating tint palette");
    }
    if(game_state->example == 12)
    {
        char lighting_buf[200];
        snprintf(lighting_buf, ArrayCount(lighting_buf), "Lighting mode: %s (Arrow Up/Down)", example12_lighting_mode_name((Example12LightingMode)game_state->example12_lighting_mode));
        draw_text(buffer, 4, 110, lighting_buf);
    }
    if(game_state->example == 13)
    {
        draw_text(buffer, 4, 110, "Stylized explosion test (R to restart)");
    }
    if(game_state->culled_triangles)
    {
        //printf("Culled triangles: %d\n", game_state->culled_triangles); 
        game_state->culled_triangles = 0; 
    }
}
