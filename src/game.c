// TODO Fix timer.h being between os_win32.h and os_win32.c. Not ergonomic!
//
// TODO See if reversed infinite perspetive is implemented correctly. Not a big problem for now, this is just visual quality
//
// TODO Investigate why some parts of the models are rendered even when they are behind other objects!
//      I think it could be because im not doing face culling altough i dont haven't build the intuition as to why this make sense.
//      Why culling would even do this?
//
// TODO At the time, rendering 4 entities takes roughly 40ms in debug build. And 20% of the time is spent doing model world camera perspective ndc viewport
//      calculations. And 80% is the actual rendering!
//      I will try to use simd to cut this 20% as much as possible
//      In release builds it seems this is closer to a 10%, but still i want to have my debug builds optimized. It's not acceptable not to!
//      
//      After adding SIMD in stage 1 I'm seeing better results. Altough nothing extremely fancy. I could try with more models or just use simd on the rasterization process
// NOTE my depth calculations are done using view space z instead of ndc z. It appears to be better to do it in NDC space
//   Reverse depth has nothing to do with this, they are orthogonal concepts!

#include "base_core.h"
#include "base_arena.h"
#include "base_os.h"
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
#include "base_string.h"
#include "base_string.c"
//#include "font_loader.h"
//#include "font_loader.c"
#undef internal
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static

#define SIMD 1
#ifndef SIMD
    #define SIMD 0  
#endif

#define PROFILE 0
#ifndef PROFILE
    #define PROFILE 0  
#endif

#define REVERSE_DEPTH_VALUE 0
#ifndef REVERSE_DEPTH_VALUE
    #define REVERSE_DEPTH_VALUE 0  
#endif

#define SHOW_DEPTH_BUFFER 0
#ifndef SHOW_DEPTH_BUFFER
    #define SHOW_DEPTH_BUFFER 0  
#endif

#define PREVIOUS_MISCONCEPTIONS 1
#define ROTATION 1

// TODO fix this is not being used when loading the teapot (models)
#define Y_UP 1

// I must also define what a backface or frontface is, is it CCW or CW?. Because in opengl you can do it.
// glFrontFace(GL_CCW);
#define CULL_FRONTFACE 0
#ifndef CULL_FRONTFACE
    #define CULL_FRONTFACE 0  
#endif

#define CULL_BACKFACE 0
#ifndef CULL_BACKFACE
    #define CULL_BACKFACE 0  
#endif

#define EDGE_FUNCTIONS 0
#define OLIVEC 0

#define EDGE_STEPPING 0
#ifndef EDGE_STEPPING
    #define EDGE_STEPPING 0
#endif


global u32 discarded;
global u32 vertices_count;
internal f32 map_range(f32 val, f32 src_range_x, f32 src_range_y, f32 dst_range_x, f32 dst_range_y)
{
    f32 t = (val - src_range_x) / (src_range_y - src_range_x);
    f32 result = dst_range_x + t * (dst_range_y - dst_range_x);
    return result;
}

internal void draw_rectangle(Software_Render_Buffer *buffer, f32 x, f32 y, f32 width, f32 height, u32 color)
{
	u32 x_min = (u32)roundf(x);
	u32 x_max = x_min + (u32)roundf(width);
	u32 y_min = (u32)roundf(y);
	u32 y_max = y_min + (u32)roundf(height);
    
	u32 *ptr = buffer->data + y_min * buffer->width + x_min;
	for(u32 y = y_min; y < y_max; y++)
	{
		u32* row = ptr;
		for(u32 x = x_min; x < x_max; x++)
		{
			*row++ = color;
		}
		ptr -= buffer->width;
	}
}

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
internal void draw_text(Software_Render_Buffer *buffer, u32 baseline, u32 x, char *text)
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

internal inline void draw_pixel_simd(Software_Render_Buffer *buffer, u32 x, u32 y, __m128i color, __m128 pass_mask)
{
    u32 *pixel_ptr = buffer->data + y * buffer->width + x;
    __m128i old_color = _mm_loadu_si128((__m128i *)pixel_ptr);
    __m128i mask = _mm_castps_si128(pass_mask);
    __m128i out_color = _mm_or_si128(_mm_and_si128(mask, color), _mm_andnot_si128(mask, old_color));
    _mm_storeu_si128((__m128i *)pixel_ptr, out_color);
}

typedef struct SIMD_Vec3 SIMD_Vec3;
struct SIMD_Vec3
{
    f32 *x;
    f32 *y;
    f32 *z;
};

typedef struct SIMD_Result SIMD_Result;
struct SIMD_Result
{
    SIMD_Vec3 *screen_space_vertices;
    f32 *inv_w;
};

typedef struct SIMD_Face SIMD_Face;
struct SIMD_Face
{
    int *v0;
    int *v1;
    int *v2;

    int *v0t;
    int *v1t;
    int *v2t;

    int *v0n;
    int *v1n;
    int *v2n;
};

typedef struct Obj_Model_SIMD Obj_Model_SIMD;
struct Obj_Model_SIMD
{
    SIMD_Vec3 vertices;
    SIMD_Face faces;
    u32 padded_vertex_count;
};

typedef struct Vec2F32 Vec2F32;
struct Vec2F32
{
    f32 x;
    f32 y;
};

inline f32 dot(Vec2F32 a, Vec2F32 b)
{
    return a.x * b.x + a.y * b.y;
}

inline f32 dotvp(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline f32 len_sq(Vec2F32 a)
{
    return dot(a, a);
}

inline f32 len(Vec2F32 a)
{
    return sqrtf(dot(a, a));
}

inline Vec2F32 norm(Vec2F32 a)
{
    f32 vlen = len(a);
    if (fabsf(vlen) < 0.0001)
    {
        return (Vec2F32){0};
    }
    Vec2F32 result = {a.x * 1.0f / vlen, a.y * 1.0f / vlen};
    return result;
}

inline Vec2F32 vec_sub(Vec2F32 a, Vec2F32 b)
{
    return (Vec2F32) {a.x - b.x, a.y - b.y};
}

inline Vec2F32 vec_add(Vec2F32 a, Vec2F32 b)
{
    return (Vec2F32) {a.x + b.x, a.y + b.y};
}

inline Vec2F32 vec_scalar(Vec2F32 b, f32 s)
{
    return (Vec2F32) {s * b.x, s * b.y};
}

inline void vec2f32_compare_and_swap_x(Vec2F32 *a, Vec2F32 *b)
{
    if (a->x > b->x)
    {
        Vec2F32 temp = *a;
        *a = *b;
        *b = temp;
    }
}

inline void vec2f32_compare_and_swap(Vec2F32 *a, Vec2F32 *b)
{
    if (a->y > b->y)
    {
        Vec2F32 temp = *a;
        *a = *b;
        *b = temp;
    }
}

void scanline_bottom_flat(Software_Render_Buffer *buffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, u32 c)
{
	// bottom flat
	Vec2F32 points_C_to_A[30000];
	u32 count_C_to_A = 0;
	Vec2F32 points_C_to_B[30000];
	u32 count_C_to_B = 0;
	{
		f32 x = C.x;
		f32 y = C.y;
		Vec2F32 aux = vec_sub(A, C);
		f32 dx_dy = (A.x - C.x) / (A.y - C.y);
		while(y >= A.y)
		{
			Vec2F32 new_pos2 = {x, y};
			points_C_to_A[count_C_to_A++] = new_pos2;
			x-=dx_dy;
			y--;
		}
	}
    
	{
		f32 x = C.x;
		f32 y = C.y;
		Vec2F32 aux = vec_sub(B, C);
		f32 dx_dy = (B.x - C.x) / (B.y - C.y);
		while(y >= B.y)
		{
			Vec2F32 new_pos2 = {x, y};
			points_C_to_B[count_C_to_B++] = new_pos2;
			x-=dx_dy;
			y--;
		}
	}
    
	{
		u32 count = 0;
        
		u32 max_count = 0;
		u32 min_count = 0;
		if(count_C_to_A > count_C_to_B)
		{
			max_count = count_C_to_A;
			min_count = count_C_to_B;
		}
		else
		{
			max_count = count_C_to_B;
			min_count = count_C_to_A;
		}
		//assert(count_C_to_A == count_C_to_B);
        
		for(u32 i = 0; i < min_count; i++)
		{
			Vec2F32 left = points_C_to_A[i];
			Vec2F32 right = points_C_to_B[i];
            //assert(points_C_to_A[i].y == points_C_to_B[i].y);
            
            vec2f32_compare_and_swap_x(&left, &right);
			if(left.x == right.x && left.y == right.y)
			{
				//framebuffer->set(left.x, left.y, c0);
			}
			else
			{
                int x0 = (int)ceilf(left.x);
                int x1 = (int)floorf(right.x);
                
                for (int x = x0; x <= x1; x++)
                    draw_pixel(buffer, x, left.y, c);
				//framebuffer->line(left.x, left.y, right.x, right.y, c0);
			}
		}
        
	}
}

void scanline_top_flat(Software_Render_Buffer *buffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, u32 c)
{
	{
		// top flat
		Vec2F32 points_A_to_B[30000];
		u32 count_A_to_B = 0;
		Vec2F32 points_A_to_C[30000];
		u32 count_A_to_C = 0;
        
		{
			f32 x = A.x;
			f32 y = A.y;
			Vec2F32 aux = vec_sub(B, A);
			f32 dx_dy = (B.x - A.x) / (B.y - A.y);
			while(y <= B.y)
			{
				Vec2F32 new_pos2 = {x, y};
				points_A_to_B[count_A_to_B++] = new_pos2;
				x+=dx_dy;
				y++;
			}
		}
        
		{
			f32 x = A.x;
			f32 y = A.y;
			Vec2F32 aux = vec_sub(C, A);
			f32 dx_dy = (C.x - A.x) / (C.y - A.y);
			while(y <= C.y)
			{
				Vec2F32 new_pos2 = {x, y};
				points_A_to_C[count_A_to_C++] = new_pos2;
				x+=dx_dy;
				y++;
			}
		}
        
		{
			u32 count = 0;
            
			u32 max_count = 0;
			u32 min_count = 0;
			if(count_A_to_B > count_A_to_C)
			{
				max_count = count_A_to_B;
				min_count = count_A_to_C;
			}
			else
			{
				max_count = count_A_to_C;
				min_count = count_A_to_B;
			}
            
		    //assert(count_A_to_C == count_A_to_B);
			for(u32 i = 0; i < min_count; i++)
			{
				Vec2F32 left = points_A_to_B[i];
				Vec2F32 right = points_A_to_C[i];
                
                //assert(left.y == right.y);
                vec2f32_compare_and_swap_x(&left, &right);
				if(left.x == right.x && left.y == right.y)
				{
					//framebuffer->set(left.x, left.y, c0);
				}
				else
				{
                    int x0 = (int)ceilf(left.x);
                    int x1 = (int)floorf(right.x);
                    
                    for (int x = x0; x <= x1; x++)
                        draw_pixel(buffer, x, left.y, c);
					//framebuffer->line(left.x, left.y, right.x, right.y, c0);
				}
			}
            
		}
	}
    
}

void draw_triangle__scanline(Software_Render_Buffer *buffer, Vec2F32 A, Vec2F32 C, Vec2F32 B, u32 c)
{
    vec2f32_compare_and_swap(&A, &B);
    vec2f32_compare_and_swap(&A, &C);
    vec2f32_compare_and_swap(&B, &C);
    
    if(A.y == B.y)
    {
        scanline_bottom_flat(buffer, A, B, C, c);
    }
    else
    {
		if(B.y == C.y)
		{
			scanline_top_flat(buffer, A, B, C, c);
		}
		else
		{
			Vec2F32 seg = vec_sub(C, A);
			f32 t = (B.y - A.y) / (C.y - A.y);
			Vec2F32 D = vec_add(A, vec_scalar(seg, t));
			scanline_top_flat(buffer, A, B, D, c);
			scanline_bottom_flat(buffer, B, D, C, c);
		}
    }
    //framebuffer->line(A.x, A.y, B.x, B.y, green);
    //framebuffer->line(C.x, C.y, B.x, B.y, green);
    //framebuffer->line(C.x, C.y, A.x, A.y, green);
    
    //framebuffer->set(A.x, A.y, green);
    //framebuffer->set(B.x, B.y, green);
    //framebuffer->set(C.x, C.y, green);
}

struct App_Memory
{
    void *memory;
};

struct App_Input
{
    f32 total_time;
    f32 dt;
};


typedef struct Entity Entity;
struct Entity
{
    const char* name;
    Obj_Model *model;
    Obj_Model_SIMD *model_simd;
    Vec3 position;
};

global u32 id;

#if PROFILE

typedef struct TimeContext TimeContext;
struct TimeContext
{
    TimeContext *next;
    const char* name;
    LONGLONG start;
    u32 id;
    b32 print_at_end_of_scope;
};

global TimeContext *time_context;

typedef struct AnchorData AnchorData;
struct AnchorData
{
    LONGLONG result;
    const char* name;
    u32 count;
};

global AnchorData anchors[150];

internal void BeginTime(const char *name, b32 print_at_end_of_scope)
{
    TimeContext *node = (TimeContext*)malloc(sizeof(TimeContext));
    node->next = time_context;
    time_context = node;
    node->print_at_end_of_scope = print_at_end_of_scope;
    if(!print_at_end_of_scope)
        node->id = __COUNTER__;

    node->name = name;
    node->start = timer_get_os_time();
}

internal void EndTime()
{
    TimeContext *node = time_context;
    if (node)
    {
		time_context = node->next;

		LONGLONG end = timer_get_os_time();
		LONGLONG result = end - node->start;
		if(node->print_at_end_of_scope)
		{
		  printf("[%s] %.2fms\n", node->name, timer_os_time_to_ms(result));
		}
		else
		{
			anchors[node->id].name = node->name;
			anchors[node->id].result += result;
			anchors[node->id].count += 1;

		}
		free(node);
    }
    else
    {
        printf("[ERROR] Wrong BeginTime/EndTime\n");
    }
}
#else
internal void BeginTime(const char *name, b32 print)
{
}

internal void EndTime()
{
}
#endif

static inline b32 olivec_normalize_triangle(int width, int height, int x1, int y1, int x2, int y2, int x3, int y3, int *lx, int *hx, int *ly, int *hy)
{
    *lx = x1;
    *hx = x1;
    if (*lx > x2) *lx = x2;
    if (*lx > x3) *lx = x3;
    if (*hx < x2) *hx = x2;
    if (*hx < x3) *hx = x3;
    if (*lx < 0) *lx = 0;
    if ((size_t) *lx >= width) return 0;;
    if (*hx < 0) return 0;;
    if ((size_t) *hx >= width) *hx = width-1;

    *ly = y1;
    *hy = y1;
    if (*ly > y2) *ly = y2;
    if (*ly > y3) *ly = y3;
    if (*hy < y2) *hy = y2;
    if (*hy < y3) *hy = y3;
    if (*ly < 0) *ly = 0;
    if ((size_t) *ly >= height) return 0;;
    if (*hy < 0) return 0;;
    if ((size_t) *hy >= height) *hy = height-1;

    return 1;
}

static inline b32 olivec_barycentric(int x1, int y1, int x2, int y2, int x3, int y3, int xp, int yp, int *u1, int *u2, int *det)
{
    *det = ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
    *u1  = ((y2 - y3)*(xp - x3) + (x3 - x2)*(yp - y3));
    *u2  = ((y3 - y1)*(xp - x3) + (x1 - x3)*(yp - y3));
    int u3 = *det - *u1 - *u2;
    return (
               (Sign(int, *u1) == Sign(int, *det) || *u1 == 0) &&
               (Sign(int, *u2) == Sign(int, *det) || *u2 == 0) &&
               (Sign(int, u3) == Sign(int, *det) || u3 == 0)
           );
}

internal f32 orient_2d(Vec2F32 a, Vec2F32 b, Vec2F32 c)
{
    // AB x CA
    return ((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x)); // 
}

// i should remove this function here and just stick with orient_2d as it seems is the normal function defined for right handed coordinates
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

static void inline clear_depth_buffer(Software_Depth_Buffer *buffer)
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

static void inline clear_screen(Software_Render_Buffer *buffer, u32 color)
{
    u32 size = buffer->width * buffer->height;
    __m128i value = _mm_set1_epi32(color);

    u32 *buffer_ptr = buffer->data;
    for(u32 i = 0; i < size; i += 4)
    {
        _mm_store_si128((__m128i*)(buffer_ptr + i), value);
    }
}

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

#if 0
internal void naive(Params *params)
{
    for (u32 y = miny; y <= maxy; y++)
    {
        for (u32 x = minx; x <= maxx; x++)
        {
            Vec3 p = (Vec3) {x, y, 0};
            Vec3 area_subtriangle_v1v2p = vec3_cross(vec3_sub(v2, v1), vec3_sub(p, v1));
            Vec3 area_subtriangle_v2v0p = vec3_cross(vec3_sub(v0, v2), vec3_sub(p, v2));
            Vec3 area_subtriangle_v0v1p = vec3_cross(vec3_sub(v1, v0), vec3_sub(p, v0));
            f32 u = vec3_magnitude(area_subtriangle_v1v2p) / 2.0f / area_of_triangle;
            f32 v = vec3_magnitude(area_subtriangle_v2v0p) / 2.0f / area_of_triangle;
            f32 w = 1.0f - u - v;
            if (vec3_dot(plane_normal_of_triangle, area_subtriangle_v1v2p) < 0 ||
                vec3_dot(plane_normal_of_triangle, area_subtriangle_v2v0p) < 0 ||
                vec3_dot(plane_normal_of_triangle, area_subtriangle_v0v1p) < 0 ) 
                {
                continue; 
                }
            

            Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(vv0_color, u), vec3_scalar(vv1_color, v)), vec3_scalar(vv2_color, w));
            f32 inv_w_interp = u * inv_w0 + v * inv_w1 + w * inv_w2;
            Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

            u32 interpolated_color_to_u32 = 0;

            interpolated_color_to_u32 |= (0xFF << 24) |
                (((u32)final_color.x) & 0xFF) << 16 |
                (((u32)final_color.y) & 0xFF) << 8 |
                (((u32)final_color.z) & 0xFF) << 0;

            draw_pixel(buffer, p.x, p.y, interpolated_color_to_u32);
        }
    }
}
    #endif

internal void olivec_params(Params *params)
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
    int lx, hx, ly, hy;
    if(olivec_normalize_triangle(params->buffer->width, params->buffer->height, v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, &lx, &hx, &ly, &hy))
    {
        for (u32 y = ly; y <= hy; y++)
        {
            for (u32 x = lx; x <= hx; x++)
            {
                int u1, u2, det;
                if(olivec_barycentric(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, x, y, &u1, &u2, &det))
                {
                    f32 inv_det = 1.0f / (f32)det;
                    f32 b0 = (f32)u1*inv_det;
                    f32 b1 = (f32)u2*inv_det;
                    //f32 b2 = (det - u1 - u2)*inv_det;
                    f32 b2 =  1.0f - b0 - b1;
                    float inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;

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
        }

    }
}

typedef struct Edge Edge;
struct Edge
{
    __m128 one_step_x;
    __m128 one_step_y;
    u32 step_x_size;
    u32 step_y_size;
};

internal __m128 edge_init(Edge *edge, Vec2F32 v0, Vec2F32 v1, Vec2F32 origin)
{
    __m128 result = {0};
    edge->step_x_size = 4;
    edge->step_y_size = 1;
    f32 A = v0.y - v1.y;
    f32 B = v1.x - v0.x;
    f32 C = v1.y*v0.x - v1.x * v0.y;
    __m128 lane_aux = _mm_set_ps(3, 2, 1, 0);
    edge->one_step_x = _mm_set1_ps(A * 4);
    edge->one_step_y = _mm_set1_ps(B * 1);
    __m128 x = _mm_add_ps(_mm_set1_ps(origin.x), lane_aux);
    __m128 y = _mm_set1_ps(origin.y);
    result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(A), x), _mm_mul_ps(_mm_set1_ps(B), y)), _mm_set1_ps(C));

    return result;
}

internal void barycentric_with_edge_stepping_SIMD(Params *params)
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
    u32 min_x = params->min_x;
    u32 max_x = params->max_x;
    u32 min_y = params->min_y;
    u32 max_y = params->max_y;

    #if 1
    Vec2F32 s0 = { floor(v0.x) + 0.5f, floor(v0.y) + 0.5f };
    Vec2F32 s1 = { floor(v1.x) + 0.5f, floor(v1.y) + 0.5f };
    Vec2F32 s2 = { floor(v2.x) + 0.5f, floor(v2.y) + 0.5f };
    Vec2F32 p = {min_x + 0.5f, min_y + 0.5f};
    #else
    Vec2F32 s0 = { v0.x, v0.y};
    Vec2F32 s1 = { v1.x, v1.y};
    Vec2F32 s2 = { v2.x, v2.y};
    Vec2F32 p = {min_x , min_y};
    #endif

    b32 e0_inc = is_top_left(s1, s2); // w0 edge
    b32 e1_inc = is_top_left(s2, s0); // w1 edge
    b32 e2_inc = is_top_left(s0, s1); // w2 edge
    f32 eps = 1e-4f;
    __m128 v_eps  = _mm_set1_ps(eps);
    __m128i lane_i      = _mm_setr_epi32(0, 1, 2, 3);
    __m128i v_max_xorig = _mm_set1_epi32((int)max_x);

    f32 area = orient_2d(s0, s1, s2);          // signed
    f32 inv_area = 1.0f / area;

    Edge e12 = {0};
    Edge e20 = {0};
    Edge e01 = {0};
    __m128 row_w0 = edge_init(&e12, s1, s2, p);
    __m128 row_w1 = edge_init(&e20, s2, s0, p);
    __m128 row_w2 = edge_init(&e01, s0, s1, p);

    u32 depth_buffer_stride = params->depth_buffer->width;
    __m128 v_one = _mm_set1_ps(1.0f);
    __m128 v_zero = _mm_set1_ps(0.0f);
    __m128 v_inv_area = _mm_set1_ps(inv_area);
    __m128 v_inv_w0 = _mm_set1_ps(inv_w0);
    __m128 v_inv_w1 = _mm_set1_ps(inv_w1);
    __m128 v_inv_w2 = _mm_set1_ps(inv_w2);
    __m128 v_v0z = _mm_set1_ps(v0.z);
    __m128 v_v1z = _mm_set1_ps(v1.z);
    __m128 v_v2z = _mm_set1_ps(v2.z);
    __m128 v_v0cx = _mm_set1_ps(v0_color.x);
    __m128 v_v1cx = _mm_set1_ps(v1_color.x);
    __m128 v_v2cx = _mm_set1_ps(v2_color.x);
    __m128 v_v0cy = _mm_set1_ps(v0_color.y);
    __m128 v_v1cy = _mm_set1_ps(v1_color.y);
    __m128 v_v2cy = _mm_set1_ps(v2_color.y);
    __m128 v_v0cz = _mm_set1_ps(v0_color.z);
    __m128 v_v1cz = _mm_set1_ps(v1_color.z);
    __m128 v_v2cz = _mm_set1_ps(v2_color.z);


    // new ones
    __m128 zz_1 = _mm_mul_ps(_mm_sub_ps(v_v1z, v_v0z), v_inv_area);
    __m128 zz_2 = _mm_mul_ps(_mm_sub_ps(v_v2z, v_v0z), v_inv_area);

    __m128 ww_1 = _mm_mul_ps(_mm_sub_ps(v_inv_w1, v_inv_w0), v_inv_area);
    __m128 ww_2 = _mm_mul_ps(_mm_sub_ps(v_inv_w2, v_inv_w0), v_inv_area);

    __m128 ccx_1 = _mm_mul_ps(_mm_sub_ps(v_v1cx, v_v0cx), v_inv_area); 
    __m128 ccx_2 =  _mm_mul_ps(_mm_sub_ps(v_v2cx, v_v0cx), v_inv_area);
    __m128 ccy_1 = _mm_mul_ps(_mm_sub_ps(v_v1cy, v_v0cy), v_inv_area); 
    __m128 ccy_2 =  _mm_mul_ps(_mm_sub_ps(v_v2cy, v_v0cy), v_inv_area);
    __m128 ccz_1 = _mm_mul_ps(_mm_sub_ps(v_v1cz, v_v0cz), v_inv_area); 
    __m128 ccz_2 =  _mm_mul_ps(_mm_sub_ps(v_v2cz, v_v0cz), v_inv_area);
    {
        for (u32 y = min_y; y < max_y; y++)
        {
            __m128 alpha = row_w0;
            __m128 beta = row_w1;
            __m128 gamma = row_w2;

            for (u32 x = min_x; x < max_x; x += 4)
            {
                #if 0
                __m128 sign_or = _mm_or_ps(_mm_or_ps(w0, w1), w2);
                __m128 inside = _mm_cmpge_ps(sign_or, v_zero);
                #else

                __m128 mask_e0 = e0_inc ? _mm_cmple_ps(alpha, v_zero) : _mm_cmplt_ps(alpha, v_eps);
                __m128 mask_e1 = e1_inc ? _mm_cmple_ps(beta, v_zero) : _mm_cmplt_ps(beta, v_eps);
                __m128 mask_e2 = e2_inc ? _mm_cmple_ps(gamma, v_zero) : _mm_cmplt_ps(gamma, v_eps);
                __m128 inside  = _mm_and_ps(_mm_and_ps(mask_e0, mask_e1), mask_e2);
                __m128i px_i    = _mm_add_epi32(_mm_set1_epi32((int)x), lane_i);
                __m128i valid_i = _mm_cmplt_epi32(px_i, v_max_xorig);
                inside = _mm_and_ps(inside, _mm_castsi128_ps(valid_i));
                #endif
                if (_mm_movemask_ps(inside))
                {
                    #if 0
                    __m128 b0 = _mm_mul_ps(alpha, v_inv_area);
                    __m128 b1 = _mm_mul_ps(beta, v_inv_area);
                    __m128 b2 = _mm_mul_ps(gamma, v_inv_area);

                    __m128 inv_w_interp = _mm_add_ps(
                        _mm_add_ps(_mm_mul_ps(b0, v_inv_w0), _mm_mul_ps(b1, v_inv_w1)),
                        _mm_mul_ps(b2, v_inv_w2));

                    __m128 depth_val = _mm_add_ps(
                        _mm_add_ps(_mm_mul_ps(b0, v_v0z), _mm_mul_ps(b1, v_v1z)),
                        _mm_mul_ps(b2, v_v2z));
                    
                    // not sure about this division, lets ignore it for now
                    // __m128 depth_val = _mm_div_ps(depth_val, inv_w_interp);

                    u32 index_base = y * depth_buffer_stride + x;
                    f32 *depth_ptr = params->depth_buffer->data + index_base;
                    __m128 depth_old = _mm_loadu_ps(depth_ptr);
                    __m128 depth_pass = _mm_cmplt_ps(depth_val, depth_old);
                    __m128 pass = _mm_and_ps(inside, depth_pass);
                    int pass_mask = _mm_movemask_ps(pass);
                    if (pass_mask)
                    {
                        __m128 depth_new = _mm_or_ps(_mm_and_ps(pass, depth_val), _mm_andnot_ps(pass, depth_old));
                        _mm_storeu_ps(depth_ptr, depth_new);

                        __m128 color_r = _mm_div_ps(
                            _mm_add_ps(
                                _mm_add_ps(_mm_mul_ps(b0, v_v0cx), _mm_mul_ps(b1, v_v1cx)),
                                _mm_mul_ps(b2, v_v2cx)),
                            inv_w_interp);
                        __m128 color_g = _mm_div_ps(
                            _mm_add_ps(
                                _mm_add_ps(_mm_mul_ps(b0, v_v0cy), _mm_mul_ps(b1, v_v1cy)),
                                _mm_mul_ps(b2, v_v2cy)),
                            inv_w_interp);
                        __m128 color_b = _mm_div_ps(
                            _mm_add_ps(
                                _mm_add_ps(_mm_mul_ps(b0, v_v0cz), _mm_mul_ps(b1, v_v1cz)),
                                _mm_mul_ps(b2, v_v2cz)),
                            inv_w_interp);

                        __m128i r_i = _mm_and_si128(_mm_cvttps_epi32(color_r), _mm_set1_epi32(0xFF));
                        __m128i g_i = _mm_and_si128(_mm_cvttps_epi32(color_g), _mm_set1_epi32(0xFF));
                        __m128i b_i = _mm_and_si128(_mm_cvttps_epi32(color_b), _mm_set1_epi32(0xFF));

                        __m128i packed_color = _mm_or_si128(
                            _mm_or_si128(_mm_set1_epi32(0xFF000000u), _mm_slli_epi32(r_i, 16)),
                            _mm_or_si128(_mm_slli_epi32(g_i, 8), b_i));

                        draw_pixel_simd(params->buffer, x, y, packed_color, pass);
                    }
                    #else
                    __m128 inv_w_interp = _mm_add_ps(
                        _mm_add_ps(v_inv_w0, 
                        _mm_mul_ps(beta, ww_1)),
                        _mm_mul_ps(gamma, ww_2));

                    __m128 w_interp = _mm_div_ps(v_one, inv_w_interp);

                    __m128 depth_val = _mm_add_ps(_mm_add_ps(v_v0z,
                         _mm_mul_ps(beta, zz_1)), 
                        _mm_mul_ps(gamma, zz_2));
                    
                    // not sure about this division, lets ignore it for now
                    //depth_val = _mm_div_ps(depth_val, inv_w_interp);

                    u32 index_base = y * depth_buffer_stride + x;
                    f32 *depth_ptr = params->depth_buffer->data + index_base;
                    __m128 depth_old = _mm_loadu_ps(depth_ptr);
                    __m128 depth_pass = _mm_cmplt_ps(depth_val, depth_old);
                    __m128 pass = _mm_and_ps(inside, depth_pass);
                    int pass_mask = _mm_movemask_ps(pass);
                    if (pass_mask)
                    {
                        __m128 depth_new = _mm_or_ps(_mm_and_ps(pass, depth_val), _mm_andnot_ps(pass, depth_old));
                        _mm_storeu_ps(depth_ptr, depth_new);

                        __m128 color_r = _mm_mul_ps(_mm_add_ps(_mm_add_ps(v_v0cx, _mm_mul_ps(beta, ccx_1)), _mm_mul_ps(gamma, ccx_2)), w_interp);
                        __m128 color_g = _mm_mul_ps(_mm_add_ps(_mm_add_ps(v_v0cy, _mm_mul_ps(beta, ccy_1)), _mm_mul_ps(gamma, ccy_2)), w_interp);
                        __m128 color_b = _mm_mul_ps(_mm_add_ps(_mm_add_ps(v_v0cz, _mm_mul_ps(beta, ccz_1)), _mm_mul_ps(gamma, ccz_2)), w_interp);

                        __m128i r_i = _mm_and_si128(_mm_cvttps_epi32(color_r), _mm_set1_epi32(0xFF));
                        __m128i g_i = _mm_and_si128(_mm_cvttps_epi32(color_g), _mm_set1_epi32(0xFF));
                        __m128i b_i = _mm_and_si128(_mm_cvttps_epi32(color_b), _mm_set1_epi32(0xFF));

                        __m128i packed_color = _mm_or_si128(
                            _mm_or_si128(_mm_set1_epi32(0xFF000000u), _mm_slli_epi32(r_i, 16)),
                            _mm_or_si128(_mm_slli_epi32(g_i, 8), b_i));

                        draw_pixel_simd(params->buffer, x, y, packed_color, pass);
                    }

                    #endif
                }
                alpha = _mm_add_ps(alpha, e12.one_step_x);
                beta = _mm_add_ps(beta, e20.one_step_x);
                gamma = _mm_add_ps(gamma, e01.one_step_x);
            }
            row_w0 = _mm_add_ps(row_w0, e12.one_step_y);
            row_w1 = _mm_add_ps(row_w1, e20.one_step_y);
            row_w2 = _mm_add_ps(row_w2, e01.one_step_y); 
        }
    }
}

internal void barycentric_with_edge_stepping_SIMD2(Params *params)
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
    u32 min_x = params->min_x;
    u32 max_x = params->max_x;
    u32 min_y = params->min_y;
    u32 max_y = params->max_y;

    Vec2F32 s0 = { floor(v0.x) + 0.5f, floor(v0.y) + 0.5f };
    Vec2F32 s1 = { floor(v1.x) + 0.5f, floor(v1.y) + 0.5f };
    Vec2F32 s2 = { floor(v2.x) + 0.5f, floor(v2.y) + 0.5f };
    f32 area = edge(s0, s1, s2);          // signed

    f32 inv_area = 1.0f / area;
    // edge stepping basically
    // For E(a,b,p): dE/dx = (b.y - a.y), dE/dy = -(b.x - a.x)
    f32 e0_dx = (s2.y - s1.y);  f32 e0_dy = -(s2.x - s1.x); // E0 = edge(s1,s2,p)
    f32 e1_dx = (s0.y - s2.y);  f32 e1_dy = -(s0.x - s2.x); // E1 = edge(s2,s0,p)
    f32 e2_dx = (s1.y - s0.y);  f32 e2_dy = -(s1.x - s0.x); // E2 = edge(s0,s1,p)

    // Evaluate at top-left of bbox (pixel center)
    f32 start_x = (f32)min_x + 0.5f;
    f32 start_y = (f32)min_y + 0.5f;
    Vec2F32 p0 = { start_x, start_y };
    f32 row_w0 = edge(s1, s2, p0);
    f32 row_w1 = edge(s2, s0, p0);
    f32 row_w2 = edge(s0, s1, p0);

    b32 e0_inc = is_top_left(s1, s2);
    b32 e1_inc = is_top_left(s2, s0);
    b32 e2_inc = is_top_left(s0, s1);

    u32 depth_buffer_stride = params->depth_buffer->width;
    f32 eps = -1e-4f;
    int lx, hx, ly, hy;
    __m128 v_e0_dx = _mm_set1_ps(e0_dx);
    __m128 v_e1_dx = _mm_set1_ps(e1_dx);
    __m128 v_e2_dx = _mm_set1_ps(e2_dx);
    __m128 v_zero = _mm_set1_ps(0.0f);
    __m128 v_eps = _mm_set1_ps(eps);
    __m128 lane = _mm_set_ps(3,2,1,0);

    {
        for (u32 y = min_y; y < max_y; y++)
        {
            u32 x = min_x;
            f32 base_w0 = row_w0;
            f32 base_w1 = row_w1;
            f32 base_w2 = row_w2;

            u32 simd_end = max_x & ~3u;
            #if 0
            for (; x < max_x && (x & 3u) != 0; x++)
            {
                f32 dx = (f32)(x - min_x);
                
                f32 w0 = base_w0 + dx * e0_dx;
                f32 w1 = base_w1 + dx * e1_dx;
                f32 w2 = base_w2 + dx * e2_dx;

                b32 inside =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);
                if (inside)
                {
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
            #endif

            
            #if 1
            for (; x + 3 < max_x; x += 4)
            {
                __m128 v_dx = _mm_add_ps(_mm_set1_ps((f32)(x - min_x)), lane);
                __m128 w0 = _mm_add_ps(_mm_set1_ps(base_w0), _mm_mul_ps(v_dx, v_e0_dx));
                __m128 w1 = _mm_add_ps(_mm_set1_ps(base_w1), _mm_mul_ps(v_dx, v_e1_dx));
                __m128 w2 = _mm_add_ps(_mm_set1_ps(base_w2), _mm_mul_ps(v_dx, v_e2_dx));

                __m128 mask_e0 = e0_inc ? _mm_cmpge_ps(w0, v_zero) : _mm_cmpgt_ps(w0, v_eps);
                __m128 mask_e1 = e1_inc ? _mm_cmpge_ps(w1, v_zero) : _mm_cmpgt_ps(w1, v_eps);
                __m128 mask_e2 = e2_inc ? _mm_cmpge_ps(w2, v_zero) : _mm_cmpgt_ps(w2, v_eps);
                __m128 inside = _mm_and_ps(_mm_and_ps(mask_e0, mask_e1), mask_e2);
                b32 inside_mask = _mm_movemask_ps(inside);
                if (inside_mask == 0) continue;
                float w0_arr[4], w1_arr[4], w2_arr[4];
                _mm_storeu_ps(w0_arr, w0);
                _mm_storeu_ps(w1_arr, w1);
                _mm_storeu_ps(w2_arr, w2);

                for (int lane = 0; lane < 4; ++lane)
                {
                    if ((inside_mask & (1 << lane)) == 0)
                        continue; // lane is outside triangle

                    u32 px = x + (u32)lane;
                    u32 index = y * depth_buffer_stride + px;
                    //f32 *depth_row = params->depth_buffer->data + y * depth_buffer_stride;

                    f32 w0s = w0_arr[lane];
                    f32 w1s = w1_arr[lane];
                    f32 w2s = w2_arr[lane];

                    f32 b0 = w0s * inv_area;
                    f32 b1 = w1s * inv_area;
                    f32 b2 = w2s * inv_area;

                    f32 inv_w_interp = b0 * inv_w0 + b1 * inv_w1 + b2 * inv_w2;
                    f32 depth_val =
                        (b0 * v0.z + b1 * v1.z + b2 * v2.z) / inv_w_interp;

                    if(depth_val < params->depth_buffer->data[index])
                    {
                        params->depth_buffer->data[index] = depth_val;

                        Vec3 interpolated_color =
                            vec3_add(
                                vec3_add(
                                    vec3_scalar(v0_color, b0),
                                    vec3_scalar(v1_color, b1)),
                                vec3_scalar(v2_color, b2));

                        Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                        u32 color_u32 = 0;
                        color_u32 |= (0xFFu << 24)
                            | (((u32)final_color.x & 0xFFu) << 16)
                            | (((u32)final_color.y & 0xFFu) << 8)
                            | (((u32)final_color.z & 0xFFu) << 0);

                        draw_pixel(params->buffer, px, y, color_u32);
                    }
                }
            }
            #endif

            #if 0
            for (; x < max_x; x++)
            {
                f32 dx = (f32)(x - min_x);
                
                f32 w0 = base_w0 + dx * e0_dx;
                f32 w1 = base_w1 + dx * e1_dx;
                f32 w2 = base_w2 + dx * e2_dx;

                b32 inside =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);
                if (inside)
                {
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
            #endif
            row_w0 += e0_dy; row_w1 += e1_dy; row_w2 += e2_dy; // step down
        }
    }
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
                b32 inside =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);
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

internal Obj_Model_SIMD obj_to_simd(Obj_Model model)
{
    Obj_Model_SIMD result = {0};
    //u32 count = model.vertex_count & ~3u; // round down
    u32 count = (model.vertex_count + 3u) & ~3u; // round up
    result.padded_vertex_count = count;

    result.vertices.x = (f32*) malloc(sizeof(f32) * count);
    result.vertices.y = (f32*) malloc(sizeof(f32) * count);
    result.vertices.z = (f32*) malloc(sizeof(f32) * count);
    for(u32 vertex_index = 0; vertex_index < model.vertex_count; vertex_index++)
    {
        Vec3 model_vertex = model.vertices[vertex_index];
        result.vertices.x[vertex_index] = model_vertex.x;
        result.vertices.y[vertex_index] = model_vertex.y;
        result.vertices.z[vertex_index] = model_vertex.z;
    }

    for(u32 vertex_index = model.vertex_count; vertex_index < count; vertex_index++)
    {
        result.vertices.x[vertex_index] = 0.0f;
        result.vertices.y[vertex_index] = 0.0f;
        result.vertices.z[vertex_index] = 0.0f;
    }

    return result;
}

typedef struct TestPackerResult TestPackerResult;
struct TestPackerResult
{
    u8* data;
    size_t size;
    u32 width;
    u32 height;
};

#if 0
internal TestPackerResult
test_packer(FontInfo *font_info)
{
    //zones[0].start = aim_timer_get_os_time();
    //zones[0].hit_count++;
    TestPackerResult result = {0};
    u32 result_width;
    u32 result_height;
    u32 result_size;
    u8* result_data;

    OS_FileReadResult font_file;
    {
        //zones[1].start = aim_timer_get_os_time();
        //zones[1].hit_count++;
        // TODO remove arena from here!
        font_file = os_file_read(arena, "C:\\Windows\\Fonts\\CascadiaMono.ttf");
        //zones[1].total_time = aim_timer_get_os_time() - zones[1].start;
    }
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
        // NOTE If `glyph_count` is not even everything goes to hell!!!
        // TODO fix this  hahah wtf is going on!?!?!?
        u32 glyph_count = (u32)('~') - (u32)('!') + 1;
        u32 glyphs_per_row = round(sqrtf(glyph_count));
        glyphs_per_row = 14;
        //glyph_count = 8;
        printf("Glyph count: %d\n", glyph_count);
        printf("Glyphs per row: %d\n", glyphs_per_row);
        u32 max_height_per_cell = face->size->metrics.height >> 6;
        u32 max_width_per_cell = face->size->metrics.max_advance >> 6;

        //u32 atlas_width = max_width_per_cell * 2;
        //u32 atlas_height = max_height_per_cell * 2;

        u32 texture_atlas_rows = round(glyph_count / glyphs_per_row);
        u32 margin_per_glyph = 2;
        u32 texture_atlas_left_padding = 2;

        result_width = (max_width_per_cell + margin_per_glyph + texture_atlas_left_padding)  * (glyphs_per_row + 3);
        result_height = max_height_per_cell * (texture_atlas_rows + 3);
        result_size = result_width * result_height;
        // TODO remove arena from here!
        result_data = (u8*) arena_push_size(arena, u8, result_size); // space for 4 glyphs

        #if 0
        u32 char_code = 0;
        u32 min_index = UINT32_MAX;
        u32 max_index = 0;
        for(;;) 
        {
            u32 glyph_index = 0;
            char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
            if (char_code == 0) 
            {
                break;
            }
            // here each char_code corresponds to ascii representantion. Also the glyph index is the same as if I did:
            // `FT_Get_Char_Index(face, 'A');`. So A would be: (65, 36)
            //printf("(%d, %d)\n", char_code, glyph_index);
            min_index = Min(min_index, glyph_index);
            max_index = Max(max_index, glyph_index);

        }
        printf("min and max indexes: (%d, %d)\n", min_index, max_index);
        #endif

        u32 prev_h = 0;
        u32 glyph_start_x = texture_atlas_left_padding;
        u32 glyph_start_y = 0;
        u32 count = 0;
        //for(u32 glyph_index = min_index; glyph_index <= max_index; glyph_index++) 
        for(u32 codepoint = '!'; codepoint <= '~'; codepoint++) 
        {

            if((count % (glyphs_per_row + 1)) == 0)
            {
                glyph_start_x = texture_atlas_left_padding;
                glyph_start_y += max_height_per_cell;
                count = 0;
            }
            u32 glyph_index = FT_Get_Char_Index(face, (u8)(codepoint));

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

                u8 *pixel = result_data + glyph_start_x + glyph_start_y * (result_width);

                u8 *ptr_uv3 = pixel;
                u8 *ptr_uv0 = pixel + (face->glyph->bitmap.rows - 1) * result_width;
                u8 *ptr_uv1 = pixel + face->glyph->bitmap.width - 1 + (face->glyph->bitmap.rows - 1) * result_width;
                u8 *ptr_uv2 = pixel + face->glyph->bitmap.width - 1;

                u8 *src_buffer = face->glyph->bitmap.buffer;
                u32 y_offset = glyph_start_y;
                for(u32 y = 0; y < face->glyph->bitmap.rows; y++)
                {
                    for(u32 x = 0; x < face->glyph->bitmap.width; x++)
                    {
                        *pixel++ = *src_buffer++;
                    }

                    y_offset += 1;
                    pixel = result_data + glyph_start_x + y_offset * (result_width);
                }
                prev_h = face->glyph->bitmap.rows;

                /*
                glm::vec2 a_uv0 = glm::vec2(0.0084, 0.2256);
                glm::vec2 a_uv1 = glm::vec2(0.0462, 0.2256);
                glm::vec2 a_uv3 = glm::vec2(0.0084, 0.1429);
                glm::vec2 a_uv2 = glm::vec2(0.0462, 0.1429);
                I know the total width
                I know the total height
                v0 = 
                */

                // for debug!!!
                #if 0
                *ptr_uv3 = 0xFF;
                *ptr_uv2 = 0xFF;
                *ptr_uv1 = 0xFF;
                *ptr_uv0 = 0xFF;
                #endif


                // this one seemed promising but only works for the height and also.. i thought 
                // this would have calclated thi value for width... so yeah
                // f32(uv3 - texture_atlas_start) / f32((max_width_per_cell + margin_per_glyph + texture_atlas_left_padding)  * (glyphs_per_row + 3))


                u32 start_x = glyph_start_x;
                int x0 = glyph_start_x;
				int x1 = glyph_start_x + face->glyph->bitmap.width ;
				int y0 = glyph_start_y;
				int y1 = glyph_start_y + face->glyph->bitmap.rows ;

				f32 u0 = (f32)(x0) / (f32)(result_width);
				f32 v0 = (f32)(y0) / (f32)(result_height);

				f32 u1 = (f32)(x1) / (f32)(result_width);
				f32 v1 = (f32)(y1) / (f32)(result_height);

                //f32 f_uv3_x = start_x / f32(result_width);
                //f32 f_uv3_x = f32((uv3 - result_data) % result_width) / f32(result_width);
                //f32 f_uv3_y = (uv3 - result_data) / result_width / f32(result_height);
                f32 f_uv3_x = u0;
                f32 f_uv3_y = v0;

                //f32 f_uv2_x = start_x / f32(result_width);
                //f32 f_uv2_x = f32((uv2 - result_data) % result_width) / f32(result_width);
                //f32 f_uv2_y = (uv2 - result_data) / result_width / f32(result_height);
                f32 f_uv2_x = u1;
                f32 f_uv2_y = v0;

                //f32 f_uv1_x = start_x / f32(result_width) + (face->glyph->bitmap.width) / f32(result_width);
                //f32 f_uv1_x = f32((uv1 - result_data) % result_width) / f32(result_width);
                //f32 f_uv1_y = (uv1 - result_data) / result_width / f32(result_height);
                f32 f_uv1_x = u1;
                f32 f_uv1_y = v1;

                //f32 f_uv0_x = start_x / f32(result_width) + (face->glyph->bitmap.width) / f32(result_width);
                //f32 f_uv0_x = f32((uv0 - result_data) % result_width) / f32(result_width);
                //f32 f_uv0_y = (uv0 - result_data) / result_width / f32(result_height);
                f32 f_uv0_x = u0;
                f32 f_uv0_y = v1;

                FontGlyph *glyph = &font_info->font_table[codepoint];


                glyph->uv0_x = f_uv0_x;
                glyph->uv1_x = f_uv1_x;
                glyph->uv2_x = f_uv2_x;
                glyph->uv3_x = f_uv3_x;

                glyph->uv0_y = f_uv0_y;
                glyph->uv1_y = f_uv1_y;
                glyph->uv2_y = f_uv2_y;
                glyph->uv3_y = f_uv3_y;

                glyph_start_x += max_width_per_cell + margin_per_glyph + face->glyph->bitmap_left;
            }
            count++;
        }
    }
    result.width = result_width;
    result.height = result_height;
    result.size = result_size;

    result.data = result_data; // space for 4 glyphs
    //zones[0].total_time = aim_timer_get_os_time() - zones[0].start ;
    return result;
}
    #endif

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
    Obj_Model model_african_head;
    Obj_Model_SIMD model_african_head_simd;
    Obj_Model model_teapot;
    Obj_Model_SIMD model_teapot_simd;
    Obj_Model model_diablo;
    Obj_Model_SIMD model_diablo_simd;
    Obj_Model model_f117;
	Entity entities[100];
	u32 entity_count;
    SIMD_Result result;
};

global u64 frame_count;
char frame_time_buf[200];
UPDATE_AND_RENDER(update_and_render)
{
    Game_State *game_state = (Game_State*)game_memory->persistent_memory;
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
                
                font_info.ascent = face->size->metrics.ascender >> 6;
                font_info.descent = - (face->size->metrics.descender >> 6); 
                font_info.line_height = face->size->metrics.height >> 6;

                {
                    for(u32 codepoint = '!'; codepoint <= '~'; codepoint++) {
                        font_info.font_table[(u32)codepoint] = font_load_glyph(face, (char)codepoint, &font_info);
                    }
                    font_info.font_table[(u32)(' ')] = font_load_glyph(face, (char)(' '), &font_info);
                }
            }
        }

        const char *filename = ".\\obj\\african_head\\african_head.obj";
        OS_FileReadResult obj = os_file_read(arena, filename);
        game_state->model_african_head = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, game_state->model_african_head.face_count);

        filename = ".\\obj\\teapot.obj";
        obj = os_file_read(arena, filename);
        game_state->model_teapot = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, game_state->model_teapot.face_count);

        filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
        obj = os_file_read(arena, filename);
        game_state->model_diablo = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, game_state->model_diablo.face_count);

        filename = ".\\obj\\f117.obj";
        obj = os_file_read(arena, filename);
        game_state->model_f117 = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, game_state->model_f117.face_count);

        game_state->model_teapot_simd = obj_to_simd(game_state->model_teapot);
        game_state->model_diablo_simd = obj_to_simd(game_state->model_diablo);
        game_state->model_african_head_simd = obj_to_simd(game_state->model_african_head);
        
        {
            Entity* entities = game_state->entities;
            u32 entity_count = game_state->entity_count;
            // TODO face_index = 1 of this model references 117 vertex which is not right as 117 > vertex_count and this should be 
            // the contents of the first face: f 1/1/1 62/2/1 61/3/1
            //entities[entity_count++] = (Entity) { .name = "enemy_2", .model = &model_f117, .position = (Vec3) {-0.8, 0.3, 2} };
            entities[entity_count++] = (Entity) { .name = "enemy_4", .model_simd = &game_state->model_diablo_simd, .model = &game_state->model_diablo, .position = (Vec3) {-0.5, -0.2, 3} }; // should in theory??? be contained between +- 2.303627
            entities[entity_count++] = (Entity) { .name = "enemy_1", .model_simd = &game_state->model_teapot_simd, .model = &game_state->model_teapot, .position = (Vec3) {3.8, -0.8, 8} };
            entities[entity_count++] = (Entity) { .name = "enemy_3", .model_simd = &game_state->model_african_head_simd, .model = &game_state->model_african_head, .position = (Vec3) {0.5, 0, 3} };
            // this configuration breaks all the models:
            // diablo
            // teapot
            // african head
            game_state->entity_count = entity_count;
            printf("Entity count: %d\n", entity_count);
        }
        #if PROFILE
        time_context = (TimeContext*)malloc(sizeof(TimeContext));
        #endif

        #if PREVIOUS_MISCONCEPTIONS
            const char *msg = "Problem: Perspective Projection misunderstanding:\n I though that every coordinate, after applying the perspective projection will be inside the NDC space, which is not true, as only applies to points inside the view frustum.\n"
            "\"The x and y coordinates of any point inside the view frustum both fall into the range [-1, 1 ] in the canonical view volume, and the z coordinates between the near and far planes are mapped to the range [0, 1]. These are called normalized device coordinates, and they are later scaled to the dimensions of the frame buffer by the viewport transform.\" -Eric Lengyel"
            "\n\n"

            "Example code:\n"
            "... after perspective projection, and then perspective divide I thought this assets would always hold\n"
            "assert(transformed_v0.x >= -1.000001 && transformed_v0.x <= 1.000001);\n"
            "assert(transformed_v0.y >= -1.000001 && transformed_v0.y <= 1.000001);\n"
            "assert(transformed_v0.z >= -NDC_EPSILON && transformed_v0.z <= 1.0f + NDC_EPSILON);\n\n"
            "... the remaining points\n";

            printf("%s\n", msg);

        #endif

        u32 total_vertices = 0;
        {
            total_vertices += game_state->model_teapot_simd.padded_vertex_count;
            total_vertices += game_state->model_diablo_simd.padded_vertex_count;
            total_vertices += game_state->model_african_head_simd.padded_vertex_count;
            //for(u32 i = 0; i < game_state->entity_count; i++)
            //{
            //    total_vertices += ;
            //}
        }
        game_state->result.screen_space_vertices = (SIMD_Vec3*)malloc(sizeof(SIMD_Vec3));
        game_state->result.screen_space_vertices->x = (f32*)malloc(sizeof(f32) * (total_vertices));
        game_state->result.screen_space_vertices->y = (f32*)malloc(sizeof(f32) * (total_vertices));
        game_state->result.screen_space_vertices->z = (f32*)malloc(sizeof(f32) * (total_vertices));
        game_state->result.inv_w = (f32*)malloc(sizeof(f32) * (total_vertices));

        {
            f32 ax = 1;
            f32 ay = 2;
            f32 bx = 1;
            f32 by = 4;
            f32 px = 0;
            f32 py = 0;
            f32 cx = 0;
            f32 cy = 0;
            f32 dx = 0;
            f32 dy = 0;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", ax, ay, bx, by, cross_2d(ax, ay, bx, by));

            Vec2F32 v0 = {1, 1};
            Vec2F32 v1 = {3, 3};
            Vec2F32 v2 = {4, 1};
            printf("-- These are equivalent \n");
            printf("hdp Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.2f\n", ax, ay, bx, by, cross_2d(v1.x - v0.x, v1.y - v0.y, v2.x - v0.x, v2.y - v0.y));
            printf("hdp Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.2f\n", ax, ay, bx, by, orient_2d(v0, v1, v2));
            printf("--- \n");
            printf("hdp Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.2f\n", ax, ay, bx, by, cross_2d(v2.x - v0.x, v2.y - v0.y, v1.x - v0.x, v1.y - v0.y));
            printf("hdp Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.2f\n", ax, ay, bx, by, orient_2d(v0, v2, v1));
            printf("-- These are equivalent \n");

            ax = -5;
            ay = 4;
            bx = -7;
            by = 5;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", ax, ay, bx, by, cross_2d(ax, ay, bx, by));
            printf("\nedge testing\n\n");

            ax = -5;
            ay = 4;
            bx = -7;
            by = 5;
            px = -6;
            py = 2;

            // (b - a) ^ (p - a)
            cx = bx - ax;
            cy = by - ay;
            dx = px - ax;
            dy = py - ay;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", cx, cy, dx, dy, cross_2d(cx, cy, dx, dy));

            // (p - a) ^ (b - a)
            cx = bx - ax;
            cy = by - ay;
            dx = px - ax;
            dy = py - ay;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", dx, dy, cx, cy, cross_2d(dx, dy, cx, cy));

            // (a - p) ^ (b - p)
            cx = ax - px;
            cy = ay - py;
            dx = bx - px;
            dy = by - py;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", cx, cy, dx, dy, cross_2d(cx, cy, dx, dy));

            // (b - p) ^ (a - p)
            cx = ax - px;
            cy = ay - py;
            dx = bx - px;
            dy = by - py;
            printf("Signed area of: (%.2f, %.2f) x (%.2f, %.2f) = %.g\n", dx, dy, cx, cy, cross_2d(dx, dy, cx, cy));

            #if 0
            #if Y_UP
                printf("Y+ goes up\n");
                #if BACKFACE_CULLING
                    printf("Discarding triangle which area is negative\n");
                    printf("Vertices must be defined in CCW order\n");
                #else
                    printf("Discarding triangle which area is positive\n");
                    printf("Vertices must be defined in CW order\n");
                #endif
            #else
                printf("Y+ goes down\n");
                #if BACKFACE_CULLING
                    printf("Discarding triangle which area is positive\n");
                    printf("Vertices must be defined in CCW order\n");
                #else
                    printf("Discarding triangle which area is negative\n");
                    printf("Vertices must be defined in CW order\n");
                #endif
            #endif
            #endif

        }
        game_memory->init = 1;
    }
    u32 black = 0xff000000;
    u32 white = 0xffffffff;
    u32 red = 0xffff0000;
    u32 green = 0xff00ff00;
    u32 blue = 0xff0000ff;
    u32 yellow = green | red;
    u32 steam_chat_background_color = 0xff1e2025;
    clear_screen(buffer, steam_chat_background_color);
    clear_depth_buffer(depth_buffer);
    
    local_persist f32 accum_dt = 0;
    local_persist f32 angle = 0;
    accum_dt += dt;
    local_persist int ks = 1;
    if(accum_dt > 5.0f)
    {
        ks = -1 * ks;
        accum_dt = 0;
    }
    angle += dt * 0.53;
    // questions:
    // what stage does the mapping from ndc to screen cordinates aka pixel buffer
    //  probably after perspective divide, altough i should really really really watch the pikuma course first because im skipping too many fundamentals sadly!
    // but first i will fix obj
    
    f32 fov = 3.141592 / 3.0; // 60 deg
    //f32 aspect_ori = (f32)buffer->height / (f32)buffer->width;
    f32 aspect = (f32)buffer->width / (f32)buffer->height;
    f32 znear = 1.0f;
    f32 zfar = 50.0f;
    f32 k = zfar / (zfar - znear);
    #if REVERSE_DEPTH_VALUE
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    printf("This shit doesnt even work. Also the depth values visualization is not clear!\n");
    Mat4 persp = mat4_make_reverse_infinite_perspective(fov, aspect, znear, zfar);
    persp = mat4_make_perspective(fov, aspect, znear, zfar);
    //persp = mat4_make_reverse_perspective(fov, aspect, znear, zfar);
    Mat4 persp_normal = mat4_make_perspective(fov, aspect, znear, zfar);

    f32 hdp = 2*znear;
    printf("Normal Projection Value Z=%.2f: %.2f\n" , hdp, ((persp_normal.m[2][2]*hdp + persp_normal.m[2][3]) / hdp));
    printf("Reversed Projection Value Z=%.2f: %.2f\n" , hdp, ((persp.m[2][2]*hdp + persp.m[2][3]) / hdp));

    hdp = (zfar - znear) * 0.5f;
    printf("Normal Projection Value Z=%.2f: %.2f\n" , hdp, ((persp_normal.m[2][2]*hdp + persp_normal.m[2][3]) / hdp));
    printf("Reversed Projection Value Z=%.2f: %.2f\n" , hdp, ((persp.m[2][2]*hdp + persp.m[2][3]) / hdp));

    hdp = zfar - 0.00032f;
    printf("Normal Projection Value Z=%.2f: %.2f\n" , hdp, ((persp_normal.m[2][2]*hdp + persp_normal.m[2][3]) / hdp));
    printf("Reversed Projection Value Z=%.2f: %.2f\n" , hdp, ((persp.m[2][2]*hdp + persp.m[2][3]) / hdp));

    hdp = (zfar - znear) * 0.212f;
    printf("Normal Projection Value Z=%.2f: %.2f\n" , hdp, ((persp_normal.m[2][2]*hdp + persp_normal.m[2][3]) / hdp));
    printf("Reversed Projection Value Z=%.2f: %.2f\n" , hdp, ((persp.m[2][2]*hdp + persp.m[2][3]) / hdp));

    #else
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
    #endif

    // rolled perspective
    f32 g = 1.0f / tan(fov * 0.5f);
    f32 g_over_aspect = g / aspect;
    // rolled perspective

    Vec3 camera_eye = (Vec3) {0.0, 0.3, 0};
    Vec3 camera_target = (Vec3) {0.0, 0.0, 1};
    Mat4 view = mat4_look_at(camera_eye, camera_target, (Vec3) {0, 1, 0});

    __m128 view_row_0;
    __m128 view_row_1;
    __m128 view_row_2;
    __m128 view_row_3;

    __m128 view_right_x;
    __m128 view_right_y;
    __m128 view_right_z;

    __m128 view_up_x;
    __m128 view_up_y;
    __m128 view_up_z;

    __m128 view_fwd_x;
    __m128 view_fwd_y;
    __m128 view_fwd_z;

    __m128 view_dot_x;
    __m128 view_dot_y;
    __m128 view_dot_z;

    {
        Vec3 eye = camera_eye;
        Vec3 target = camera_target;
        Vec3 up = (Vec3) {0, 1, 0};

        Vec3 z = vec3_sub(target, eye);
        z = vec3_normalize(z);
        Vec3 x = vec3_cross(up, z);
        x = vec3_normalize(x);
        Vec3 y = vec3_cross(z, x);
        
        view_right_x = _mm_set1_ps(x.x);
        view_right_y = _mm_set1_ps(x.y);
        view_right_z = _mm_set1_ps(x.z);

        view_up_x = _mm_set1_ps(y.x);
        view_up_y = _mm_set1_ps(y.y);
        view_up_z = _mm_set1_ps(y.z);

        view_fwd_x = _mm_set1_ps(z.x);
        view_fwd_y = _mm_set1_ps(z.y);
        view_fwd_z = _mm_set1_ps(z.z);


        view_dot_x = _mm_set1_ps(-vec3_dot(x, eye));
        view_dot_y = _mm_set1_ps(-vec3_dot(y, eye));
        view_dot_z = _mm_set1_ps(-vec3_dot(z, eye));
        //view_row_0 = _mm_set1_ps( x.x, x.y, x.z, -vec3_dot(x, eye) );
        //view_row_1 = _mm_setr_ps( y.x, y.y, y.z, -vec3_dot(y, eye) );
        //view_row_2 = _mm_setr_ps( z.x, z.y, z.z, -vec3_dot(z, eye) );
        //view_row_3 = _mm_setr_ps(0, 0, 0, 1);
    }
    //view = mat4_identity();
    
    LONGLONG model_now = timer_get_os_time();
	f32 c = cos(angle);
	f32 s = sin(angle);
    __m128 m_c = _mm_set1_ps(c);
    __m128 m_s = _mm_set1_ps(s);
    __m128 m_negs = _mm_set1_ps(-s);
	f32 c_90 = cos(3.14 / 2.0f);
	f32 s_90 = sin(3.14 / 2.0f);
    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    discarded = 0;
    /*
    f32 w_v0 = transformed_v0.z;
    f32 w_v1 = transformed_v1.z;
    f32 w_v2 = transformed_v2.z;

    // g_over_aspect is used because g is the focal length, which is where
    // the projection plane is: z = g
    // and because the idea is to get this coordinates into the view volume
    // it must go from [-aspect, aspect] to [-1, 1]
    // x_proj / g = x / z => x_proj = g/z * x this gives [-aspect to -aspect] so then
    // i divide it by aspect. So resulting form is: g_over_aspect * x
    transformed_v0.x = g_over_aspect * transformed_v0.x;
    transformed_v0.y = g * transformed_v0.y;
    transformed_v0.z = -znear * k * transformed_v0.z;

    transformed_v1.x = g_over_aspect * transformed_v1.x;
    transformed_v1.y = g * transformed_v1.y;
    transformed_v1.z = -znear * k * transformed_v1.z;

    transformed_v2.x = g_over_aspect * transformed_v2.x;
    transformed_v2.y = g * transformed_v2.y;
    transformed_v2.z = -znear * k * transformed_v2.z;


    transformed_v0.x /= w_v0;
    transformed_v0.y /= w_v0;

    transformed_v1.x /= w_v1;
    transformed_v1.y /= w_v1;

    transformed_v2.x /= w_v2;
    transformed_v2.y /= w_v2;
    */
    BeginTime("processing models", 1);
    #if SIMD
    {
        //for (u32 entity = 0; entity < entity_count; entity++)
        f32 fov = 3.141592 / 3.0; // 60 deg
        f32 g = 1.0f / tan(fov * 0.5f);
        f32 aspect = (f32)buffer->width / (f32)buffer->height;
        f32 znear = 1.0f;
        f32 zfar = 50.0f;
        f32 k = zfar / (zfar - znear);
        f32 g_over_aspect = g / aspect;
        f32 minus_znear_times_k = -znear * k;

        f32 one = 1.0f;
        f32 half = 0.5f;

        __m128 half_width  = _mm_set1_ps(0.5f * (f32)buffer->width);
        __m128 half_height = _mm_set1_ps(0.5f * (f32)buffer->height);
        __m128 neg_half_width  = _mm_set1_ps(-0.5f * (f32)buffer->width);
        __m128 neg_half_height = _mm_set1_ps(-0.5f * (f32)buffer->height);
        for (u32 entity = 0; entity < 3; entity++)
        {
            Entity* e = game_state->entities + entity;
            Obj_Model_SIMD *model = e->model_simd;
            {
                f32 scale = 0.4f;
                __m128 scalar = _mm_load_ps1(&scale);

                __m128 x_translation = _mm_set1_ps(e->position.x);
                __m128 y_translation = _mm_set1_ps(e->position.y);
                __m128 z_translation = _mm_set1_ps(e->position.z);

                for(u32 vertex_index = 0; vertex_index < model->padded_vertex_count; vertex_index+=4)
                {
                    //if (vertex_index + 4 >= e->model->vertex_count) break;
                    //BeginTime("vertex transformation", 0);
                    f32 *x_prime = &model->vertices.x[vertex_index];
                    f32 *y_prime = &model->vertices.y[vertex_index];
                    f32 *z_prime = &model->vertices.z[vertex_index];
                    __m128 packed_x_prime = _mm_loadu_ps(x_prime);
                    __m128 packed_y_prime = _mm_loadu_ps(y_prime);
                    __m128 packed_z_prime = _mm_loadu_ps(z_prime);

                    // World space
                    
                        
                    // rotation
                       // result.x = p.x * c + p.z * s;
                       // result.y = p.y;
                       // result.z = -p.x * s + p.z * c;

                    __m128 aux_packed_x_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, m_c), _mm_mul_ps(packed_z_prime, m_s));
                    //packed_y_prime = _mm_mul_ps(packed_y_prime, scalar);
                    __m128 aux_packed_z_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, m_negs), _mm_mul_ps(packed_z_prime, m_c));
                    packed_x_prime = aux_packed_x_prime;
                    packed_z_prime = aux_packed_z_prime;
                    // rotation
                    

                    packed_x_prime = _mm_mul_ps(packed_x_prime, scalar);
                    packed_y_prime = _mm_mul_ps(packed_y_prime, scalar);
                    packed_z_prime = _mm_mul_ps(packed_z_prime, scalar);

                    packed_x_prime = _mm_add_ps(packed_x_prime, x_translation);
                    packed_y_prime = _mm_add_ps(packed_y_prime, y_translation);
                    packed_z_prime = _mm_add_ps(packed_z_prime, z_translation);


                    // view matrix
                    {
                        __m128 packed_x_aux_prime = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(view_right_x, packed_x_prime), _mm_mul_ps(view_right_y, packed_y_prime)), _mm_mul_ps(view_right_z, packed_z_prime)), view_dot_x);
                        __m128 packed_y_aux_prime = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(view_up_x, packed_x_prime), _mm_mul_ps(view_up_y, packed_y_prime)), _mm_mul_ps(view_up_z, packed_z_prime)), view_dot_y);
                        __m128 packed_z_aux_prime = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(view_fwd_x, packed_x_prime), _mm_mul_ps(view_fwd_y, packed_y_prime)), _mm_mul_ps(view_fwd_z, packed_z_prime)), view_dot_z);
                        packed_x_prime = packed_x_aux_prime;
                        packed_y_prime = packed_y_aux_prime;
                        packed_z_prime = packed_z_aux_prime;

                    }
                    __m128 view_space_packed_z = packed_z_prime;
                    

                    // perspective matrix
                    // after: in clip space
                    packed_x_prime = _mm_mul_ps(packed_x_prime, _mm_load_ps1(&g_over_aspect));
                    packed_y_prime = _mm_mul_ps(packed_y_prime, _mm_load_ps1(&g));
                    packed_z_prime = _mm_add_ps(_mm_mul_ps(packed_z_prime, _mm_load_ps1(&k)), _mm_load_ps1(&minus_znear_times_k));
                    __m128 one    = _mm_set1_ps(1.0f);
                    __m128 zero   = _mm_set1_ps(0.0f);

                    // mask = 0xFFFFFFFF where w != 0, else 0
                    __m128 mask = _mm_cmpneq_ps(view_space_packed_z, zero);

                    // Numerator masked → 1.0 only in lanes where we will divide
                    __m128 num = _mm_and_ps(one, mask);

                    // Use 1.0 in masked-off lanes so invalid lanes evaluate to 0 instead of NaN.
                    __m128 den = _mm_or_ps(_mm_and_ps(view_space_packed_z, mask), _mm_andnot_ps(mask, one));

                    // Now division executes only in valid lanes;
                    // invalid lanes see 0/0 → undefined but masked off
                    __m128 inv_w = _mm_div_ps(num, den);

                    // Apply perspective divide only where mask == 1
                    packed_x_prime = _mm_or_ps(
                        _mm_and_ps(_mm_mul_ps(packed_x_prime, inv_w), mask),   // updated lanes
                        _mm_andnot_ps(mask, packed_x_prime));                  // original lanes

                    packed_y_prime = _mm_or_ps(
                        _mm_and_ps(_mm_mul_ps(packed_y_prime, inv_w), mask),
                        _mm_andnot_ps(mask, packed_y_prime));

                    packed_z_prime = _mm_or_ps(
                        _mm_and_ps(_mm_mul_ps(packed_z_prime, inv_w), mask),
                        _mm_andnot_ps(mask, packed_z_prime));


                    // Viewport Space
                    // I was using this one! It should change the inside tests. Which doesnt seem like it...
                    // For Y down:
                    //     this: _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
                    //     should be equivalent to: v.x = (v.x * 0.5f + 0.5f) * buffer->width;
                    //     after reordering: v.x = v.x * buffer->width / 2 + buffer->width / 2
                    //     or: v.x = v.x * half_width + half_width
                    //     same for v.y
                    // packed_x_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
                    // packed_y_prime = _mm_add_ps(_mm_mul_ps(packed_y_prime, half_height), half_height);

                    // For Y up:
                    //     v.x = (1.0f - (v.x * 0.5f + 0.5f)) * buffer->width;
                    //     1.0f - v.x * 0.5 - 0.5f => (0.5f - v.x * 0.5)
                    //     (0.5 - 0.5f * v.x) * buffer->width => half_width - half_width * v.x
                    
                    packed_x_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, neg_half_width), half_width);
                    packed_y_prime = _mm_add_ps(_mm_mul_ps(packed_y_prime, neg_half_height), half_height);

                    
                    _mm_storeu_ps(game_state->result.screen_space_vertices->x + vertices_count + vertex_index, packed_x_prime);
                    _mm_storeu_ps(game_state->result.screen_space_vertices->y + vertices_count + vertex_index, packed_y_prime); 
                    _mm_storeu_ps(game_state->result.screen_space_vertices->z + vertices_count + vertex_index, packed_z_prime); 
                    _mm_storeu_ps(game_state->result.inv_w + vertices_count + vertex_index, inv_w); 
                }


                for(int face_index = 0; face_index < e->model->face_count; face_index++)
                {
                    {
                        Face face = e->model->faces[face_index];

                        Vec3 v0 = (Vec3) {game_state->result.screen_space_vertices->x[face.v[0] - 1 + vertices_count], game_state->result.screen_space_vertices->y[face.v[0] - 1 + vertices_count], game_state->result.screen_space_vertices->z[face.v[0] - 1 + vertices_count]};
                        Vec3 v1 = (Vec3) {game_state->result.screen_space_vertices->x[face.v[1] - 1 + vertices_count], game_state->result.screen_space_vertices->y[face.v[1] - 1 + vertices_count], game_state->result.screen_space_vertices->z[face.v[1] - 1 + vertices_count]};
                        Vec3 v2 = (Vec3) {game_state->result.screen_space_vertices->x[face.v[2] - 1 + vertices_count], game_state->result.screen_space_vertices->y[face.v[2] - 1 + vertices_count], game_state->result.screen_space_vertices->z[face.v[2] - 1 + vertices_count]};

                        f32 inv_w0 = game_state->result.inv_w[face.v[0] - 1 + vertices_count];
                        f32 inv_w1 = game_state->result.inv_w[face.v[1] - 1 + vertices_count];
                        f32 inv_w2 = game_state->result.inv_w[face.v[2] - 1 + vertices_count];

                        f32 min_x = Min(Min(v0.x, v1.x), v2.x);
                        f32 min_y = Min(Min(v0.y, v1.y), v2.y);
                        f32 max_x = Max(Max(v0.x, v1.x), v2.x);
                        f32 max_y = Max(Max(v0.y, v1.y), v2.y);
                        min_x = ClampBot(min_x, 0);
                        max_x = ClampTop(max_x, buffer->width);
                        min_y = ClampBot(min_y, 0);
                        max_y = ClampTop(max_y, buffer->height);

                        Vec3 new_vv0_color = vec3_scalar(vv0_color, inv_w0);
                        Vec3 new_vv1_color = vec3_scalar(vv1_color, inv_w1);
                        Vec3 new_vv2_color = vec3_scalar(vv2_color, inv_w2);

                        /////draw_triangle__scanline(buffer, v0, v1, v2, color);
                        Params params = 
                        {
                            view, persp,
                            v0, v1, v2,
                            new_vv0_color, new_vv1_color, new_vv2_color,
                            inv_w0, inv_w1, inv_w2,
                            min_x, max_x, min_y, max_y
                        };

						params.buffer = buffer;
						params.depth_buffer = depth_buffer;

                        {
                            //BeginTime("barycentric calculation", 0);
                            barycentric_with_edge_stepping_SIMD(&params);
                        }
                    }
                }
                vertices_count += model->padded_vertex_count;
            }
        }
    }
    #else
    {
        #if 1
        {
            //for (u32 entity = 0; entity < entity_count; entity++)
            for (u32 entity = 0; entity < 1; entity++)
            {
                Entity* e = game_state->entities + entity;
                Obj_Model *model = e->model;
                if(model->is_valid)
                {
                    for(int face_index = 0; face_index <= model->face_count; face_index++)
                    {
                        BeginTime("rendering models", 0);
                        {
                            u32 color = blue;
                            Face face = model->faces[face_index];
                            Vec3 v0 = model->vertices[face.v[0] - 1];
                            Vec3 v1 = model->vertices[face.v[1] - 1];
                            Vec3 v2 = model->vertices[face.v[2] - 1];

                            if (model->has_normals)
                            {
                                Vec3 n0 = model->vertices[face.vn[0] - 1];
                                Vec3 n1 = model->vertices[face.vn[1] - 1];
                                Vec3 n2 = model->vertices[face.vn[2] - 1];
                            }


                            #if ROTATION
                            //if(model == &model_f117)
                            //{

                            //    v0 = vec3_rotate_z(v0, c_90, s_90);
                            //    v1 = vec3_rotate_z(v1, c_90, s_90);
                            //    v2 = vec3_rotate_z(v2, c_90, s_90);

                            //    v0 = vec3_rotate_y(v0, c, s);
                            //    v1 = vec3_rotate_y(v1, c, s);
                            //    v2 = vec3_rotate_y(v2, c, s);
                            //}
                            //else
                            {
                                v0 = vec3_rotate_y(v0, c, s);
                                v1 = vec3_rotate_y(v1, c, s);
                                v2 = vec3_rotate_y(v2, c, s);
                            }
                            #endif
                            
                            // World space
                            v0 = vec3_scalar(v0, 0.5f);
                            v1 = vec3_scalar(v1, 0.5f);
                            v2 = vec3_scalar(v2, 0.5f);
                            
                            v0.x += e->position.x;
                            v0.y += e->position.y;
                            v0.z += e->position.z;
                            
                            v1.x += e->position.x;
                            v1.y += e->position.y;
                            v1.z += e->position.z;

                            v2.x += e->position.x;
                            v2.y += e->position.y;
                            v2.z += e->position.z;
                            
                            // view space
                            Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
                            Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
                            Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
                            Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};
                            Vec3 transformed_v1_v3 = (Vec3) {transformed_v1.x, transformed_v1.y, transformed_v1.z};
                            Vec3 transformed_v2_v3 = (Vec3) {transformed_v2.x, transformed_v2.y, transformed_v2.z};

                            f32 inv_w0;
                            f32 inv_w1;
                            f32 inv_w2;
                            // remember that the projection matrix stores de viewspace z value in its w
                            // but the result vector is in clip space so z is in clip space, not in 
                            // viewspace, they are not the same zz
                            transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
                            transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
                            transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
                            //BeginTime("transformation", 0);
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


                            v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
                            v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
                            v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
                            #if Y_UP
                            v0.y = (1.0f - (v0.y * 0.5f + 0.5f)) * buffer->height;
                            v1.y = (1.0f - (v1.y * 0.5f + 0.5f)) * buffer->height;
                            v2.y = (1.0f - (v2.y * 0.5f + 0.5f)) * buffer->height;
                            #else
                            v0.y = ((v0.y * 0.5f + 0.5f)) * buffer->height;
                            v1.y = ((v1.y * 0.5f + 0.5f)) * buffer->height;
                            v2.y = ((v2.y * 0.5f + 0.5f)) * buffer->height;
                            #endif

                            f32 min_x = Min(Min(v0.x, v1.x), v2.x);
                            f32 min_y = Min(Min(v0.y, v1.y), v2.y);
                            f32 max_x = Max(Max(v0.x, v1.x), v2.x);
                            f32 max_y = Max(Max(v0.y, v1.y), v2.y);
                            min_x = ClampBot(min_x, 0);
                            max_x = ClampTop(max_x, buffer->width);
                            min_y = ClampBot(min_y, 0);
                            max_y = ClampTop(max_y, buffer->height);

                            Vec3 new_vv0_color = vec3_scalar(vv0_color, inv_w0);
                            Vec3 new_vv1_color = vec3_scalar(vv1_color, inv_w1);
                            Vec3 new_vv2_color = vec3_scalar(vv2_color, inv_w2);
                            EndTime();

                            /////draw_triangle__scanline(buffer, v0, v1, v2, color);
                            Params params = 
                            {
                                view, persp,
                                v0, v1, v2,
                                new_vv0_color, new_vv1_color, new_vv2_color,
                                inv_w0, inv_w1, inv_w2,
                                min_x, max_x, min_y, max_y
                            };

                            params.buffer = buffer;
                            params.depth_buffer = depth_buffer;
                            barycentric_with_edge_stepping(&params);
                            //olivec_params(&params);
                            //naive()
                        }
                    }
                }
            }
        }
        #else


            // CCW o CW it doesnt even matter!
            Vec3 vv0_color = (Vec3) {255, 0, 0};
            Vec3 vv1_color = (Vec3) {0, 255, 0};
            Vec3 vv2_color = (Vec3) {0, 0, 255};

            // CW
            // this are valid when i do the flipping of y in the viewport mapping 
            // skewed
            //Vec4 v0_v4 = (Vec4) {-0.2, 0.2, 1.0, 1.0f};
            //Vec4 v1_v4 = (Vec4) {-0.4, 0.5, 1.2, 1.0f};
            //Vec4 v2_v4 = (Vec4) {-0.6, 0.2, 1.0, 1.0f};
            //center at origin
            //Vec4 v0_v4 = (Vec4) {0.0,  0.7, 35.0, 1.0f};
            //Vec4 v1_v4 = (Vec4) {-0.6, -0.4, 1.0, 1.0f};
            //Vec4 v2_v4 = (Vec4) {0.6, -0.4, 1.0, 1.0f};
        

            /*
                Okay so there are two possible interpretations of this.
                
                I can define the vertices of some triangle on a clockwise or in a counter-clockwise manner. This will depend on how I'm thinking about the vertices.
                Because I use the wedge product which is defined in a right-hand coordinate system to see if triangles are counter or clockwise formed. It matters that
                these vertices are specified in this coordinate system. So if the wedge product is positive then they are counter-clockwise and if the wedge product is negative
                they are clockwise.

                But the stage where I perform the culling matters. Because I said I'm using the wedge product as defined in a right-hand coordinate system. If the vertices
                end up in another system then I have to reinterpret the meaning of the signed area.
                Right now I'm not sure where I should apply face culling so I'm doing it after viewport transform. Now... if I defined the vertex in clockwise order but at the time
                I was thinking of the screen having coordinates Y+ up and X+ right then that means the area is positive if they are counter-clockwise.
                But if I flipped them that means that now, the vertices should be interpreted 
            
            */
                // v0 -> v1 -> v2 could easily be counter or clock wise defined. We dont know that. This will depend on wether 0,0 is at the top and Y grows down
                // or if 0, 0 is at the bot and Y grows up. When writting this comment im using Y_UP = 0 so 0,0 is at the top Y grows down. So for this case:
                // these are defined counter clockwise. What that also means is when i call orient_2d later the signed area is going to be negative for this vertices.
                // Why? Because of two things. First Y grows down, and in that setup, this vertices are counter clockwise
                Vec4 v0_v4 = (Vec4) {0.0,  0.7, 50.0, 1.0f}; // red
                Vec4 v1_v4 = (Vec4) {0.6, -0.4, 1.0, 1.0f};  // green
                Vec4 v2_v4 = (Vec4) {-0.6, -0.4, 1.0, 1.0f}; // blue
            // if i get them smaller i get a much better framerate!


            v0_v4 = mat4_mul_vec4(view, v0_v4);
            v1_v4 = mat4_mul_vec4(view, v1_v4);
            v2_v4 = mat4_mul_vec4(view, v2_v4);

            v0_v4 = mat4_mul_vec4(persp, v0_v4);
            v1_v4 = mat4_mul_vec4(persp, v1_v4);
            v2_v4 = mat4_mul_vec4(persp, v2_v4);

            f32 inv_w0 = 1.0f / v0_v4.w;
            f32 inv_w1 = 1.0f / v1_v4.w;
            f32 inv_w2 = 1.0f / v2_v4.w;
            if(v0_v4.w != 0)
            {
                v0_v4.x *= inv_w0;
                v0_v4.y *= inv_w0;
                v0_v4.z *= inv_w0;
            }
            
            if(v1_v4.w != 0)
            {
                v1_v4.x *= inv_w1;
                v1_v4.y *= inv_w1;
                v1_v4.z *= inv_w1;
            }
            
            if(v2_v4.w != 0)
            {
                v2_v4.x *= inv_w2;
                v2_v4.y *= inv_w2;
                v2_v4.z *= inv_w2;
            }


            // DONE flip it it viewport space to see if the culling holds for sign area which shouldnt
            //  - yes it changes the sign of the area, pretty cool!

            // viewport mapping
            #if Y_UP
			v0_v4.x = (v0_v4.x * 0.5f + 0.5f) * buffer->width;
			v0_v4.y = (1.0f - (v0_v4.y * 0.5f + 0.5f)) * buffer->height;

			v1_v4.x = (v1_v4.x * 0.5f + 0.5f) * buffer->width;
			v1_v4.y = (1.0f - (v1_v4.y * 0.5f + 0.5f)) * buffer->height;

			v2_v4.x = (v2_v4.x * 0.5f + 0.5f) * buffer->width;
			v2_v4.y = (1.0f - (v2_v4.y * 0.5f + 0.5f)) * buffer->height;
            #else

			v0_v4.x = (v0_v4.x * 0.5f + 0.5f) * buffer->width;
			v0_v4.y = (v0_v4.y * 0.5f + 0.5f) * buffer->height;

			v1_v4.x = (v1_v4.x * 0.5f + 0.5f) * buffer->width;
			v1_v4.y = (v1_v4.y * 0.5f + 0.5f) * buffer->height;

			v2_v4.x = (v2_v4.x * 0.5f + 0.5f) * buffer->width;
			v2_v4.y = (v2_v4.y * 0.5f + 0.5f) * buffer->height;
            #endif

            // this is wrong later on when calculting the area
            // without edge functions, because im mixing spaces, one is screen an another one is NDC depth
            Vec3 v0 = (Vec3) {v0_v4.x, v0_v4.y, v0_v4.z};
            Vec3 v1 = (Vec3) {v1_v4.x, v1_v4.y, v1_v4.z};
            Vec3 v2 = (Vec3) {v2_v4.x, v2_v4.y, v2_v4.z};
            f32 minx = Min(Min(v0.x, v1.x), v2.x);
            f32 miny = Min(Min(v0.y, v1.y), v2.y);
            f32 maxx = Max(Max(v0.x, v1.x), v2.x);
            f32 maxy = Max(Max(v0.y, v1.y), v2.y);

			vv0_color = vec3_scalar(vv0_color, inv_w0);
			vv1_color = vec3_scalar(vv1_color, inv_w1);
			vv2_color = vec3_scalar(vv2_color, inv_w2);
            Params params = {view, persp, v0, v1, v2, vv0_color, vv1_color, vv2_color, inv_w0, inv_w1, inv_w2, minx, maxx, miny, maxy};
            params.buffer = buffer;
            params.depth_buffer = depth_buffer;
            //barycentric_with_edge_stepping(&params);

            Vec3 plane_normal_of_triangle = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
            f32 area_of_parallelogram = vec3_magnitude(plane_normal_of_triangle);
            f32 area_of_triangle = area_of_parallelogram / 2.0f;
            #if 0

            #if EDGE_FUNCTIONS
            Vec2F32 s0 = { v0_v4.x, v0_v4.y };
            Vec2F32 s1 = { v1_v4.x, v1_v4.y };
            Vec2F32 s2 = { v2_v4.x, v2_v4.y };

            float area = edge(s0, s1, s2);          // signed
            if (area == 0) return;                 // degenerate
            float inv_area = 1.0f / area;

            #if EDGE_STEPPING
            // edge stepping basically
            // For E(a,b,p): dE/dx = (b.y - a.y), dE/dy = -(b.x - a.x)
            float e0_dx = (s2.y - s1.y);  float e0_dy = -(s2.x - s1.x); // E0 = edge(s1,s2,p)
            float e1_dx = (s0.y - s2.y);  float e1_dy = -(s0.x - s2.x); // E1 = edge(s2,s0,p)
            float e2_dx = (s1.y - s0.y);  float e2_dy = -(s1.x - s0.x); // E2 = edge(s0,s1,p)

            // Evaluate at top-left of bbox (pixel center)
            float start_x = (float)minx + 0.5f;
            float start_y = (float)miny + 0.5f;
            Vec2F32 p0 = { start_x, start_y };
            float row_w0 = edge(s1, s2, p0);
            float row_w1 = edge(s2, s0, p0);
            float row_w2 = edge(s0, s1, p0);
            #endif

            for (u32 i = 0; i <= 10; i++)
            {
                for (u32 y = miny; y <= maxy; y++)
                {
                    #if EDGE_STEPPING
                    float w0 = row_w0;
                    float w1 = row_w1;
                    float w2 = row_w2;
                    #endif

                    for (u32 x = minx; x <= maxx; x++)
                    {
                        #if EDGE_STEPPING
                            // if i remvoe this first condition it doesnt render anymore. So this is front-face culling
                            //if (!((w0 < 0 || w1 < 0 || w2 < 0) && (w0 > 0 || w1 > 0 || w2 > 0)))
                            #if Y_UP
                            // this only works is Y_UP == 1
                            if (!((w0 > 0 || w1 > 0 || w2 > 0)))
                            #else
                            // this only works is Y_UP == 0
                            if (!((w0 < 0 || w1 < 0 || w2 < 0)))
                            #endif
                            {
                                float b0 = w0 * inv_area;
                                float b1 = w1 * inv_area;
                                float b2 = w2 * inv_area;
                                f32 depth = b0 * v0_v4.z + b1 * v1_v4.z + b2 * v2_v4.z;
                                #if REVERSE_DEPTH_VALUE
                                if(depth > depth_buffer->data[y * buffer->width + x])
                                #else
                                if(depth < depth_buffer->data[y * buffer->width + x])
                                #endif
                                {
                                    depth_buffer->data[y * buffer->width + x] = depth;
                                    float inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;
                                    Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(vv0_color, b0), vec3_scalar(vv1_color, b1)), vec3_scalar(vv2_color, b2));
                                    Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                                    u32 interpolated_color_to_u32 = 0;

                                    interpolated_color_to_u32 |= (0xFF << 24) |
                                        (((u32)final_color.x) & 0xFF) << 16 |
                                        (((u32)final_color.y) & 0xFF) << 8 |
                                        (((u32)final_color.z) & 0xFF) << 0;

                                    draw_pixel(buffer, x, y, interpolated_color_to_u32);

                                    #if SHOW_DEPTH_BUFFER
                                    //float z_linear =  zfar / depth;
                                    //float v = Clamp((z_linear - znear) / (zfar - znear), 0, 1);

                                    u32 interpolated_depth_to_u32 = 0;
                                    interpolated_depth_to_u32 |= (0xFF << 24) |
                                     (((u32)(depth * 255.0f)) & 0xFF) << 16 |
                                     (((u32)(depth * 255.0f)) & 0xFF) << 8 |
                                     (((u32)(depth * 255.0f)) & 0xFF) << 0; 


                                    draw_pixel(buffer, x, y, interpolated_depth_to_u32);
                                    #endif
                                }
                            }
                        #else
                            Vec2F32 p_v2 = (Vec2F32) {x, y};
                            float w0 = edge(s1, s2, p_v2);
                            float w1 = edge(s2, s0, p_v2);
                            float w2 = edge(s0, s1, p_v2);
                            // same sign test (works for both windings)
                            if ((w0 < 0 || w1 < 0 || w2 < 0) && (w0 > 0 || w1 > 0 || w2 > 0))
                                continue;
                            float b0 = w0 * inv_area;
                            float b1 = w1 * inv_area;
                            float b2 = w2 * inv_area;
                            float inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;
                            Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(vv0_color, b0), vec3_scalar(vv1_color, b1)), vec3_scalar(vv2_color, b2));
                            Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                            u32 interpolated_color_to_u32 = 0;

                            interpolated_color_to_u32 |= (0xFF << 24) |
                                (((u32)final_color.x) & 0xFF) << 16 |
                                (((u32)final_color.y) & 0xFF) << 8 |
                                (((u32)final_color.z) & 0xFF) << 0;

                            draw_pixel(buffer, p_v2.x, p_v2.y, interpolated_color_to_u32);
                        #endif
                    #if EDGE_STEPPING
                    w0 += e0_dx; w1 += e1_dx; w2 += e2_dx; // step right
                    #endif
                    }
                #if EDGE_STEPPING
                row_w0 += e0_dy; row_w1 += e1_dy; row_w2 += e2_dy; // step right
                #endif
                }
            }
            #endif

            #else
                #if OLIVEC
                    f32 z1 = v0.z / 1.0f;
                    f32 z2 = v1.z / 1.0f;
                    f32 z3 = v2.z / 1.0f;
                    int lx, hx, ly, hy;
                    //if(olivec_normalize_triangle(buffer->width, buffer->height, v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, &lx, &hx, &ly, &hy))
                    {
                        {
                            for (u32 y = miny; y <= maxy; y++)
                            {
                                for (u32 x = minx; x <= maxx; x++)
                                {
                                    int u1, u2, det;
                                    if(olivec_barycentric(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, x, y, &u1, &u2, &det))
                                    {
                                        f32 inv_det = 1.0f / (f32)det;
                                        f32 b0 = (f32)u1*inv_det;
                                        f32 b1 = (f32)u2*inv_det;
                                        //f32 b2 = (det - u1 - u2)*inv_det;
                                        f32 b2 =  1.0f - b0 - b1;
                                        float inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;

                                        Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(vv0_color, b0), vec3_scalar(vv1_color, b1)), vec3_scalar(vv2_color, b2));
                                        Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                                        u32 interpolated_color_to_u32 = 0;

                                        interpolated_color_to_u32 |= (0xFF << 24) |
                                            (((u32)final_color.x) & 0xFF) << 16 |
                                            (((u32)final_color.y) & 0xFF) << 8 |
                                            (((u32)final_color.z) & 0xFF) << 0;
                                        draw_pixel(buffer, x, y, interpolated_color_to_u32);
                                    }
                                }
                            }
                        }
                    }
                #else
                // NAIVE
                    f32 area_maybe = orient_2d((Vec2F32){v0.x, v0.y}, (Vec2F32){v1.x, v1.y}, (Vec2F32){v2.x, v2.y});
                    b32 skip = 0;
                    #if Y_UP
                        #if CULL_BACKFACE
                        if(area_maybe < 0.0f) // cw
                        {
                            skip = 1;
                        }
                        #endif
                        #if CULL_FRONTFACE
                        if(area_maybe > 0.0f) // ccw
                        {
                            skip = 1;
                        }
                        #endif
                    #else
                        #if CULL_BACKFACE
                        if(area_maybe > 0.0f) // cw
                        {
                            skip = 1;
                        }
                        #endif
                        #if CULL_FRONTFACE
                        if(area_maybe < 0.0f) // ccw
                        {
                            skip = 1;
                        }
                        #endif
                    #endif
                    if (!skip)
                    {
                        for (u32 y = miny; y <= maxy; y++)
                        {
                            for (u32 x = minx; x <= maxx; x++)
                            {
                                Vec3 p = (Vec3) {x, y, 0};
                                Vec3 area_subtriangle_v1v2p = vec3_cross(vec3_sub(v2, v1), vec3_sub(p, v1));
                                Vec3 area_subtriangle_v2v0p = vec3_cross(vec3_sub(v0, v2), vec3_sub(p, v2));
                                Vec3 area_subtriangle_v0v1p = vec3_cross(vec3_sub(v1, v0), vec3_sub(p, v0));

                                f32 u = vec3_magnitude(area_subtriangle_v1v2p) / 2.0f / area_of_triangle;
                                f32 v = vec3_magnitude(area_subtriangle_v2v0p) / 2.0f / area_of_triangle;
                                f32 w = 1.0f - u - v;
                                if (vec3_dot(plane_normal_of_triangle, area_subtriangle_v1v2p) < 0 ||
                                    vec3_dot(plane_normal_of_triangle, area_subtriangle_v2v0p) < 0 ||
                                    vec3_dot(plane_normal_of_triangle, area_subtriangle_v0v1p) < 0 ) 
                                    {
                                    continue; 
                                    }
                                

                                Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(vv0_color, u), vec3_scalar(vv1_color, v)), vec3_scalar(vv2_color, w));
                                f32 inv_w_interp = u * inv_w0 + v * inv_w1 + w * inv_w2;
                                Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                                u32 interpolated_color_to_u32 = 0;

                                interpolated_color_to_u32 |= (0xFF << 24) |
                                    (((u32)final_color.x) & 0xFF) << 16 |
                                    (((u32)final_color.y) & 0xFF) << 8 |
                                    (((u32)final_color.z) & 0xFF) << 0;

                                draw_pixel(buffer, p.x, p.y, interpolated_color_to_u32);
                            }
                        }
                    }
                #endif
            #endif
        #endif
    }
    #endif
    EndTime();
#if PROFILE
    for (u32 i = 0; i < 100; i++)
    {
        if (anchors[i].result == 0) continue;
        printf("[%s] hit per frame: %d, time total per frame: %.5fms  time avg per frame: %.5fms\n", anchors[i].name, anchors[i].count, timer_os_time_to_ms(anchors[i].result), timer_os_time_to_ms(anchors[i].result) / anchors[i].count);
        anchors[i].result = 0;
        anchors[i].count = 0;
    }
    printf("Discarded trigs: %d of %d total trigs\n", discarded, model_teapot.face_count);
#endif
    {
        // render text
        if(frame_count % 300 == 0)
        {
            LONGLONG model_end = timer_get_os_time();
            LONGLONG result = model_end - model_now;
            snprintf(frame_time_buf, 300, "per frame time: %.2fms\n", timer_os_time_to_ms(result));
        }
        draw_text(buffer, 10, 4, frame_time_buf);
        char buf[300];
        snprintf(buf, 300, "frame count: %lld\n", frame_count);
        draw_text(buffer, 30, 4, buf);
        //draw_rectangle(buffer, 120, 200, 10, 10, 0xFFFF0000);
    }
    
    //printf("per frame time: %.2fms\n", timer_os_time_to_ms(result));
    vertices_count = 0;
    frame_count++;
}
