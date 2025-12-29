#include "base_core.h"
#include "base_os.h"
#include "os_win32.h"
// TODO this order aint nice, fix it
#include "timer.h"
#include "os_win32.c"
#include "base_math.h"
#include "timer.c"
#include "obj.h"
#include <assert.h>

#define REVERSE_DEPTH_VALUE 0
#ifndef REVERSE_DEPTH_VALUE
    #define REVERSE_DEPTH_VALUE 0  
#endif

#define SHOW_DEPTH_BUFFER 0
#ifndef SHOW_DEPTH_BUFFER
    #define SHOW_DEPTH_BUFFER 0  
#endif
#define FLIPPED_Y 1
#define PREVIOUS_MISCONCEPTIONS 1
#define ROTATION 1

#define EDGE_FUNCTIONS 0
#define OLIVEC 1

#define EDGE_STEPPING 0
#ifndef EDGE_STEPPING
    #define EDGE_STEPPING 0
#endif


global u32 discarded;
f32 map_range(f32 val, f32 src_range_x, f32 src_range_y, f32 dst_range_x, f32 dst_range_y)
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
		ptr += buffer->width;
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

global Obj_Model model_african_head;
global Obj_Model model_teapot;
global Obj_Model model_diablo;
global Obj_Model model_f117;

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
    Vec3 position;
};

global Entity entities[100];
global u32 entity_count;
global u32 id;

typedef struct AnchorData AnchorData;
struct AnchorData
{
    LONGLONG result;
    const char* name;
    u32 count;
};

global AnchorData anchors[150];

typedef struct TimeContext TimeContext;
struct TimeContext
{
    TimeContext *next;
    const char* name;
    LONGLONG start;
    u32 id;
    b32 print;
};

#if 1
global TimeContext *time_context;
internal void BeginTime(const char *name, b32 print)
{
    TimeContext *node = (TimeContext*)malloc(sizeof(TimeContext));
    node->next = time_context;
    time_context = node;
    node->print = print;
    if(!print)
        node->id = __COUNTER__;

    node->name = name;
    node->start = timer_get_os_time();
}

internal void EndTime()
{
    TimeContext *node = time_context;
    time_context = node->next;

    LONGLONG end = timer_get_os_time();
    LONGLONG result = end - node->start;
    if(node->print)
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
#else
global TimeContext *time_context;
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

static inline clear_depth_buffer(Software_Depth_Buffer *buffer)
{

    #if REVERSE_DEPTH_VALUE
    memset((void*)buffer->data, 0x00, buffer->width * buffer->height * 4);
    #else
    memset((void*)buffer->data, 0xFF, buffer->width * buffer->height * 4);
    #endif
}

static inline clear_screen(Software_Render_Buffer *buffer, u32 color)
{
    draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, color);
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
    f32 minx;
    f32 maxx;
    f32 miny;
    f32 maxy;
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
    f32 minx = params->minx;
    f32 maxx = params->maxx;
    f32 miny = params->miny;
    f32 maxy = params->maxy;
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
    f32 minx = params->minx;
    f32 maxx = params->maxx;
    f32 miny = params->miny;
    f32 maxy = params->maxy;

    Vec2F32 s0 = { floor(v0.x) + 0.5f, floor(v0.y) + 0.5f };
    Vec2F32 s1 = { floor(v1.x) + 0.5f, floor(v1.y) + 0.5f };
    Vec2F32 s2 = { floor(v2.x) + 0.5f, floor(v2.y) + 0.5f };
    f32 area = edge(s0, s1, s2);          // signed
    #if 0
    if (area < 0.0f)
    {
        
        Swap(v1, v2);
        Swap(v1_color, v2_color);
        Swap(inv_w1, inv_w2);

        Swap(s1, s2);

        area = -area;
    }
        #endif

    f32 inv_area = 1.0f / area;
    // edge stepping basically
    // For E(a,b,p): dE/dx = (b.y - a.y), dE/dy = -(b.x - a.x)
    f32 e0_dx = (s2.y - s1.y);  float e0_dy = -(s2.x - s1.x); // E0 = edge(s1,s2,p)
    f32 e1_dx = (s0.y - s2.y);  float e1_dy = -(s0.x - s2.x); // E1 = edge(s2,s0,p)
    f32 e2_dx = (s1.y - s0.y);  float e2_dy = -(s1.x - s0.x); // E2 = edge(s0,s1,p)

    // Evaluate at top-left of bbox (pixel center)
    f32 start_x = (float)minx + 0.5f;
    f32 start_y = (float)miny + 0.5f;
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
        for (u32 y = miny; y < maxy; y++)
        {
            f32 w0 = row_w0;
            f32 w1 = row_w1;
            f32 w2 = row_w2;

            for (u32 x = minx; x < maxx; x++)
            {
                b32 inside =
                    (e0_inc ? w0 >= 0.f : w0 > eps) &&
                    (e1_inc ? w1 >= 0.f : w1 > eps) &&
                    (e2_inc ? w2 >= 0.f : w2 > eps);
                if (inside)
                {
                    #if 0
                    #if FLIPPED_Y
                    // this only works is flipped_y == 1
                    if (!((w0 > 0 || w1 > 0 || w2 > 0)))
                    #else
                    // this only works is flipped_y == 0
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

UPDATE_AND_RENDER(update_and_render)
{
    // remove this must come from the invoker
    local_persist b32 init = 0;
    if(!init)
    {
        timer_init();
        const char *filename = ".\\obj\\african_head\\african_head.obj";
        OS_FileReadResult obj = os_file_read(filename);
        model_african_head = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_african_head.face_count);

        filename = ".\\obj\\teapot.obj";
        obj = os_file_read(filename);
        model_teapot = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_teapot.face_count);

        filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
        obj = os_file_read(filename);
        model_diablo = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_diablo.face_count);

        filename = ".\\obj\\f117.obj";
        obj = os_file_read(filename);
        model_f117 = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_f117.face_count);
        
        {
            entities[entity_count++] = (Entity) { .name = "enemy_1", .model = &model_teapot, .position = (Vec3) {0.0, -0.8, 3} };
            entities[entity_count++] = (Entity) { .name = "enemy_2", .model = &model_f117, .position = (Vec3) {-0.8, 0.3, 2} };
            entities[entity_count++] = (Entity) { .name = "enemy_3", .model = &model_diablo, .position = (Vec3) {0.5, 0, 1} };
            entities[entity_count++] = (Entity) { .name = "enemy_4", .model = &model_african_head, .position = (Vec3) {-0.5, -0.2, 1} };
            // should in theory??? be contained between +- 2.303627
        }
        printf("Entity count: %d\n", entity_count);

        time_context = (TimeContext*)malloc(sizeof(TimeContext));
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
        init = 1;
    }
    u32 black = 0xff000000;
    u32 white = 0xffffffff;
    u32 red = 0xffff0000;
    u32 green = 0xff00ff00;
    u32 blue = 0xff0000ff;
    u32 yellow = green | red;
    u32 steam_chat_background_color = 0xff1e2025;
    //draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, 0xff111f0f);
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
    angle += ks * dt;
    // questions:
    // what stage does the mapping from ndc to screen cordinates aka pixel buffer
    //  probably after perspective divide, altough i should really really really watch the pikuma course first because im skipping too many fundamentals sadly!
    // but first i will fix obj
    
    Vec3 camera_eye = (Vec3) {0, 0.0, 0};
    Vec3 camera_target = (Vec3) {0, 0, 1};
    camera_target = vec3_sub(camera_target, camera_eye);
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
    // TODO fix this shit i dont see the good depth values. just a retarded invertion 
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

    Mat4 view = mat4_look_at(camera_eye, camera_target, (Vec3) {0, 1, 0});
    //view = mat4_identity();
    
    LONGLONG model_now = timer_get_os_time();
	f32 c = cos(angle);
	f32 s = sin(angle);
	f32 c_90 = cos(3.14 / 2.0f);
	f32 s_90 = sin(3.14 / 2.0f);
    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    discarded = 0;
    {
        BeginTime("rendering models", 1);
        #if 1
        {
            for (u32 entity = 0; entity < entity_count; entity++)
            //for (u32 entity = 0; entity < 1; entity++)
            {
                Entity* e = entities + entity;
                Obj_Model *model = e->model;
                if(model->is_valid)
                {
                    for(int face_index = 1; face_index <= model->face_count; face_index++)
                    {
                        u32 color = blue;
                        Face face = model->faces[face_index];
                        Vec3 v0 = model->vertices[face.v[0]];
                        Vec3 v1 = model->vertices[face.v[1]];
                        Vec3 v2 = model->vertices[face.v[2]];

                        if (model->has_normals)
                        {
                            Vec3 n0 = model->vertices[face.vn[0]];
                            Vec3 n1 = model->vertices[face.vn[1]];
                            Vec3 n2 = model->vertices[face.vn[2]];
                        }


                        #if ROTATION
                        if(model == &model_f117)
                        {

                            v0 = vec3_rotate_z(v0, c_90, s_90);
                            v1 = vec3_rotate_z(v1, c_90, s_90);
                            v2 = vec3_rotate_z(v2, c_90, s_90);

                            v0 = vec3_rotate_y(v0, c, s);
                            v1 = vec3_rotate_y(v1, c, s);
                            v2 = vec3_rotate_y(v2, c, s);
                        }
                        else
                        {
                            v0 = vec3_rotate_y(v0, c, s);
                            v1 = vec3_rotate_y(v1, c, s);
                            v2 = vec3_rotate_y(v2, c, s);
                        }
                        #endif
                        
                        // World space
                        v0 = vec3_scalar(v0, 0.2f);
                        v1 = vec3_scalar(v1, 0.2f);
                        v2 = vec3_scalar(v2, 0.2f);
                        
                        v0.x += e->position.x;
                        v0.y += e->position.y;
                        v0.z += e->position.z;
                        
                        v1.x += e->position.x;
                        v1.y += e->position.y;
                        v1.z += e->position.z;

                        v2.x += e->position.x;
                        v2.y += e->position.y;
                        v2.z += e->position.z;
                        
                        

                        #if 0
                        {
                            Vec3 N = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
                            Vec3 V = vec3_sub(camera_eye, v0);
                            f32 dot_result = vec3_dot(N, V);
                            // clip everything that has a normal pointing in the opposite direction to vertex to camera
                            if(dot_result <= 0)
                            {
                                continue;
                            }
                        }
                        #endif

                        
                        // view space
                        Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
                        Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
                        Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
                        Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};
                        Vec3 transformed_v1_v3 = (Vec3) {transformed_v1.x, transformed_v1.y, transformed_v1.z};
                        Vec3 transformed_v2_v3 = (Vec3) {transformed_v2.x, transformed_v2.y, transformed_v2.z};
                        Vec3 N = vec3_cross(vec3_sub(transformed_v1_v3, transformed_v0_v3), vec3_sub(transformed_v2_v3, transformed_v0_v3));
                        //if(N.z >= 0 ) continue;
                        if(N.z >= 0 )
                        {
                            // cull
                            //color = blue;
                            //continue;
                        }
                        else
                        {
                            //color = green;
                        }



                        // why is this not working?
                        if(transformed_v0.z <= znear)
                        {
                            //continue;
                        }
                        if(transformed_v1.z <= znear)
                        {
                            //continue;
                        }
                        if(transformed_v2.z <= znear)
                        {
                            //continue;
                        }
                        f32 inv_w0;
                        f32 inv_w1;
                        f32 inv_w2;
                        
                        #if 1
                        {
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


                        }
                        #else
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
                        #endif
                        {
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
                            #if FLIPPED_Y
                            v0.y = (1.0f - (v0.y * 0.5f + 0.5f)) * buffer->height;
                            v1.y = (1.0f - (v1.y * 0.5f + 0.5f)) * buffer->height;
                            v2.y = (1.0f - (v2.y * 0.5f + 0.5f)) * buffer->height;
                            #else
                            v0.y = ((v0.y * 0.5f + 0.5f)) * buffer->height;
                            v1.y = ((v1.y * 0.5f + 0.5f)) * buffer->height;
                            v2.y = ((v2.y * 0.5f + 0.5f)) * buffer->height;
                            #endif

                            
                            f32 minx = Min(Min(v0.x, v1.x), v2.x);
                            f32 miny = Min(Min(v0.y, v1.y), v2.y);
                            f32 maxx = Max(Max(v0.x, v1.x), v2.x);
                            f32 maxy = Max(Max(v0.y, v1.y), v2.y);

                            Vec3 new_vv0_color = vec3_scalar(vv0_color, inv_w0);
                            Vec3 new_vv1_color = vec3_scalar(vv1_color, inv_w1);
                            Vec3 new_vv2_color = vec3_scalar(vv2_color, inv_w2);

                            /////draw_triangle__scanline(buffer, v0, v1, v2, color);
                            Params params = {view, persp, v0, v1, v2, new_vv0_color, new_vv1_color, new_vv2_color, inv_w0, inv_w1, inv_w2, minx, maxx, miny, maxy};
                            params.buffer = buffer;
                            params.depth_buffer = depth_buffer;
                            barycentric_with_edge_stepping(&params);
                            //olivec_params(&params);
                            //naive()

                            //draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Barycentric);
                            
                            //Triangle_Mesh triangle = {v0, v1, v2, blue};
                            //prepare_scene_for_this_frame(&triangle, width, height);
                        }
                        //EndTime();
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

            //  CCW
            Vec4 v0_v4 = (Vec4) {0.0,  0.7, 50.0, 1.0f};
            Vec4 v1_v4 = (Vec4) {0.6, -0.4, 1.0, 1.0f};
            Vec4 v2_v4 = (Vec4) {-0.6, -0.4, 1.0, 1.0f};
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
            #if FLIPPED_Y
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
            barycentric_with_edge_stepping(&params);

            #if 0
            Vec3 plane_normal_of_triangle = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
            f32 area_of_parallelogram = vec3_magnitude(plane_normal_of_triangle);
            f32 area_of_triangle = area_of_parallelogram / 2.0f;

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
                            #if FLIPPED_Y
                            // this only works is flipped_y == 1
                            if (!((w0 > 0 || w1 > 0 || w2 > 0)))
                            #else
                            // this only works is flipped_y == 0
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
                        for (u32 i = 0; i <= 10; i++)
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
                    for (u32 i = 0; i <= 10; i++)
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
            
        EndTime();
    }
    for (u32 i = 0; i < 20; i++)
    {
        if (anchors[i].result == 0) continue;
        printf("[%s] hit per frame: %d, time_total: %.5fms  time_average: %.5fms\n", anchors[i].name, anchors[i].count, timer_os_time_to_ms(anchors[i].result), timer_os_time_to_ms(anchors[i].result) / anchors[i].count);
        anchors[i].result = 0;
        anchors[i].count = 0;
    }
    printf("Discarded trigs: %d of %d total trigs\n", discarded, model_teapot.face_count);
    //LONGLONG model_end = timer_get_os_time();
    //LONGLONG result = model_end - model_now;
    
    //printf("per frame time: %.2fms\n", timer_os_time_to_ms(result));
}