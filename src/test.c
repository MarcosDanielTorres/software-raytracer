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
#include "timer.c"
#include "obj.h"
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
                #if 1
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

static inline b32 is_top_left_edge(float dx, float dy)
{
    return (dy > 0) || (dy == 0 && dx < 0);
}
internal void barycentric_with_edge_stepping(Params *params)
{
    Vec3 v0 = params->v0;
    Vec3 v1 = params->v1;
    Vec3 v2 = params->v2;
    Vec3 v0_color = params->v0_color;
    Vec3 v1_color = params->v1_color;
    Vec3 v2_color = params->v2_color;
    f32 inv_w0 = params->inv_w0;
    f32 inv_w1 = params->inv_w1;
    f32 inv_w2 = params->inv_w2;
    f32 min_x = params->min_x;
    f32 max_x = params->max_x;
    f32 min_y = params->min_y;
    f32 max_y = params->max_y;

    Vec2F32 s0 = { floor(v0.x) + 0.5f, floor(v0.y) + 0.5f };
    Vec2F32 s1 = { floor(v1.x) + 0.5f, floor(v1.y) + 0.5f };
    Vec2F32 s2 = { floor(v2.x) + 0.5f, floor(v2.y) + 0.5f };
    //Vec2F32 s0 = { v0.x + 0.5f, (v0.y) + 0.5f };
    //Vec2F32 s1 = { (v1.x) + 0.5f, (v1.y) + 0.5f };
    //Vec2F32 s2 = { (v2.x) + 0.5f, (v2.y) + 0.5f };
    f32 area = edge(s0, s1, s2);          // signed
    #if Y_UP
    if (area < 0.0f) // is CW
    {
        //Swap(v1, v2);
        //Swap(v1_color, v2_color);
        //Swap(inv_w1, inv_w2);

        //Swap(s1, s2);

        //area = -area;
    }
    #else
    if(area > 0.0f) // is CW
    {
    }
    #endif

    f32 inv_area = 1.0f / area;
    // edge stepping basically
    // For E(a,b,p): dE/dx = (b.y - a.y), dE/dy = -(b.x - a.x)
    f32 e0_dx = (s2.y - s1.y);  float e0_dy = -(s2.x - s1.x); // E0 = edge(s1,s2,p)
    f32 e1_dx = (s0.y - s2.y);  float e1_dy = -(s0.x - s2.x); // E1 = edge(s2,s0,p)
    f32 e2_dx = (s1.y - s0.y);  float e2_dy = -(s1.x - s0.x); // E2 = edge(s0,s1,p)

    // Evaluate at top-left of bbox (pixel center)
    f32 start_x = (float)min_x + 0.5f;
    f32 start_y = (float)min_y + 0.5f;
    Vec2F32 p0 = { start_x, start_y };
    f32 row_w0 = edge(s1, s2, p0);
    f32 row_w1 = edge(s2, s0, p0);
    f32 row_w2 = edge(s0, s1, p0);

    b32 e0_inc = is_top_left(s1, s2);
    b32 e1_inc = is_top_left(s2, s0);
    b32 e2_inc = is_top_left(s0, s1);

    const f32 eps = -1e-4f;
    int lx, hx, ly, hy;
    {
        for (u32 y = min_y; y < max_y; y++)
        {
            f32 w0 = row_w0;
            f32 w1 = row_w1;
            f32 w2 = row_w2;

            for (u32 x = min_x; x < max_x; x++)
            {
                // this checks if ccw
                b32 inside_pos =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);

                // this checks if cw
                b32 inside_neg =
                    (e0_inc ? w0 <= 0.f : w0 < -eps) &&
                    (e1_inc ? w1 <= 0.f : w1 < -eps) &&
                    (e2_inc ? w2 <= 0.f : w2 < -eps);
                b32 inside = inside_pos || inside_neg;

                //b32 inside = inside_pos;
                if (inside)
                {
                    #if 0
                    #if Y_UP
                    // this only works is Y_UP == 1
                    if (!((w0 > 0 || w1 > 0 || w2 > 0)))
                    #else
                    // this only works is Y_UP == 0
                    if (!((w0 < 0 || w1 < 0 || w2 < 0)))
                    #endif
                    #endif
                    {
                        // area here is always less than 0
                        //printf("%d\n", area > 0 ? 1 : 0);
                        f32 b0 = w0 * inv_area;
                        f32 b1 = w1 * inv_area;
                        f32 b2 = w2 * inv_area;
                        f32 inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;
                        f32 depth = (b0 * v0.z + b1 * v1.z + b2 * v2.z) / inv_w_interp;
                        if(depth < params->depth_buffer->data[y * params->buffer->width + x])
                        {
                            params->depth_buffer->data[y * params->buffer->width + x] = depth;
                            Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(v0_color, b0), vec3_scalar(v1_color, b1)), vec3_scalar(v2_color, b2));
                            Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                            u32 interpolated_color_to_u32 = 0;

                            interpolated_color_to_u32 |= (0xFF << 24) |
                                (((u32)final_color.x) & 0xFF) << 16 |
                                (((u32)final_color.y) & 0xFF) << 8 |
                                (((u32)final_color.z) & 0xFF) << 0;

                            draw_pixel(params->buffer, x, y, interpolated_color_to_u32);

                        }
                        }
                }
                w0 += e0_dx; w1 += e1_dx; w2 += e2_dx; // step right
            }
        row_w0 += e0_dy; row_w1 += e1_dy; row_w2 += e2_dy; // step down
        }
    }
}

void provisionary_block(Software_Render_Buffer *buffer, Software_Depth_Buffer *depth_buffer, Triangle* triangles, u32 count, Mat4 view, Mat4 persp)
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
        if(sign_area > 0)
        {
            // culling
            //continue;
        }

        ////// viewport transform //////
        v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
        v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
        v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
        v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;
        v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;
        v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;
        ////// viewport transform //////

        f32 min_x = Min(Min(v0.x, v1.x), v2.x);
        f32 min_y = Min(Min(v0.y, v1.y), v2.y);
        f32 max_x = Max(Max(v0.x, v1.x), v2.x);
        f32 max_y = Max(Max(v0.y, v1.y), v2.y);
        min_x = ClampBot(min_x, 0);
        max_x = ClampTop(max_x, buffer->width);
        min_y = ClampBot(min_y, 0);
        max_y = ClampTop(max_y, buffer->height);

        Vec3 new_vv0_color = (Vec3){255.0f, 0.0f, 0.0f};
        Vec3 new_vv1_color = (Vec3){0.0f, 255.0f, 0.0f};
        Vec3 new_vv2_color = (Vec3){0.0f, 0.0f, 255.0f};

        Params params = 
        {
            view, persp,
            v0, v1, v2,
            new_vv0_color, new_vv1_color, new_vv2_color,
            1, 1, 1,
            min_x, max_x, min_y, max_y
        };

        params.buffer = buffer;
        params.depth_buffer = depth_buffer;
        barycentric_with_edge_stepping(&params);
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

typedef struct Game_State Game_State;
struct Game_State
{
    Arena *arena;
    FontInfo font_info;
};

UPDATE_AND_RENDER(update_and_render)
{
    Game_State *game_state = (Game_State*)game_memory->persistent_memory;
    font_info = game_state->font_info;
    if(!game_memory->init)
    {
        printf("init\n");
        game_state->arena = arena_alloc_with_base(game_memory->persistent_memory_size - sizeof(Game_State), (u8*)game_memory->persistent_memory + sizeof(Game_State));
        Arena *arena = game_state->arena;

        timer_init();
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
        game_memory->init = 1;
    }
    u32 steam_chat_background_color = 0xff1e2025;
    clear_screen(buffer, steam_chat_background_color);
    clear_depth_buffer(depth_buffer);
    
    // My idea here is the following. I'm trying to understand how the culling works so I know that if i have a triangle given in CCW
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

    u32 example = 1;
    if(example == 1)
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
        provisionary_block(buffer, depth_buffer, triangles, 2, view, persp);
    }
    if(example == 2)
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
        f32 znear = 1.0f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block(buffer, depth_buffer, triangles, 2, view, persp);
    }
    if(example == 3)
    {
        // Example 3
        // This could be like example 1 with a camera... seems good right?
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
        f32 znear = 1.0f;
        f32 zfar = 50.0f;
        Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
        provisionary_block(buffer, depth_buffer, triangles, 2, view, persp);
    }

    char buf[200];
    snprintf(buf, 200, "Running example: %d", example);
    draw_text(buffer, 4, 20, buf);
}
