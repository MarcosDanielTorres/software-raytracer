#include "base_core.h"
#include "base_os.h"
#include "base_math.h"
#include "os_win32.h"
#include "timer.h"
#include "os_win32.c"
#include "timer.c"
#include "obj.h"
#include <assert.h>
#define EDGE_FUNCTIONS 0
#define EDGE_STEPPING 0

// TODO inspect probably messing load_ps and load_ps1. 
// TODO these values should be equal. See obj_to_simd i was loding it wrong basically
//   model->vertices[0].x	5.94552631e-25	float
//   model->vertices[0].y	6.376e-43#DEN	float
//   model->vertices[0].z	6.00167743e-25	float
//   model_simd->vertices.x[0]	5.94552631e-25	float
//   model_simd->vertices.y[0]	5.94552631e-25	float
//   model_simd->vertices.z[0]	5.94552631e-25	float


#include <immintrin.h>

global Software_Render_Buffer *buffer;


typedef struct SIMD_Vec3 SIMD_Vec3;
struct SIMD_Vec3
{
    f32 *x;
    f32 *y;
    f32 *z;
};

typedef struct Obj_Model_SIMD Obj_Model_SIMD;
struct Obj_Model_SIMD
{
    SIMD_Vec3 vertices;
};

global Obj_Model_SIMD model_simd;

internal inline Vec3
vec3_from_vec4(Vec4 a)
{
    Vec3 result = {a.x, a.y, a.z};
    return result;
}

internal inline Vec4
vec4_from_vec3(Vec3 a)
{
    Vec4 result = {a.x, a.y, a.z, 0.0};
    return result;
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
    Obj_Model_SIMD *model_simd;
};

internal void create_and_apply_mvp(Params *params)
{
    Vec4 v0 = vec4_from_vec3(params->v0);
    Vec4 v1 = vec4_from_vec3(params->v1);
    Vec4 v2 = vec4_from_vec3(params->v2);
    f32 *inv_w0 = &params->inv_w0;
    f32 *inv_w1 =  &params->inv_w1;
    f32 *inv_w2 = &params->inv_w2;

    Mat4 view = mat4_identity();
    f32 fov = 3.141592 / 3.0; // 60 deg
    f32 aspect = (f32)buffer->width / (f32)buffer->height;
    f32 znear = 1.0f;
    f32 zfar = 50.0f;
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);

    v0 = mat4_mul_vec4(view, v0);
    v1 = mat4_mul_vec4(view, v1);
    v2 = mat4_mul_vec4(view, v2);

    v0 = mat4_mul_vec4(persp, v0);
    v1 = mat4_mul_vec4(persp, v1);
    v2 = mat4_mul_vec4(persp, v2);

    if(v0.w != 0)
    {
        *inv_w0 = 1.0f / v0.w;
        v0.x *= *inv_w0;
        v0.y *= *inv_w0;
        v0.z *= *inv_w0;
    }
    
    if(v1.w != 0)
    {
        *inv_w1 = 1.0f / v1.w;
        v1.x *= *inv_w1;
        v1.y *= *inv_w1;
        v1.z *= *inv_w1;
    }
    
    if(v2.w != 0)
    {
        *inv_w2 = 1.0f / v2.w;
        v2.x *= *inv_w2;
        v2.y *= *inv_w2;
        v2.z *= *inv_w2;
    }

    v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
    v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;

    v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
    v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;

    v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
    v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;

    params->v0 = vec3_from_vec4(v0);
    params->v1 = vec3_from_vec4(v1);
    params->v2 = vec3_from_vec4(v2);
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

internal inline b32 olivec_barycentric(int x1, int y1, int x2, int y2, int x3, int y3, int xp, int yp, int *u1, int *u2, int *det)
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

internal void olivec_version(Params *params)
{
    Vec3 v0 = params->v0;
    Vec3 v1 = params->v1;
    Vec3 v2 = params->v2;
    Vec3 v0_color = params->v0_color;
    Vec3 v1_color = params->v0_color;
    Vec3 v2_color = params->v0_color;
    f32 inv_w0 = params->inv_w0;
    f32 inv_w1 = params->inv_w1;
    f32 inv_w2 = params->inv_w2;
    f32 minx = params->minx;
    f32 maxx = params->maxx;
    f32 miny = params->miny;
    f32 maxy = params->maxy;

    int lx, hx, ly, hy;
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

                Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(v0_color, b0), vec3_scalar(v1_color, b1)), vec3_scalar(v2_color, b2));
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

internal inline f32 edge(Vec2F32 a, Vec2F32 b, Vec2F32 p)
{
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

internal void barycentric_with_edge_stepping(Params *params)
{
    Vec3 v0 = params->v0;
    Vec3 v1 = params->v1;
    Vec3 v2 = params->v2;
    Vec3 v0_color = params->v0_color;
    Vec3 v1_color = params->v0_color;
    Vec3 v2_color = params->v0_color;
    f32 inv_w0 = params->inv_w0;
    f32 inv_w1 = params->inv_w1;
    f32 inv_w2 = params->inv_w2;
    f32 minx = params->minx;
    f32 maxx = params->maxx;
    f32 miny = params->miny;
    f32 maxy = params->maxy;

    Vec2F32 s0 = { v0.x, v0.y };
    Vec2F32 s1 = { v1.x, v1.y };
    Vec2F32 s2 = { v2.x, v2.y };
    float area = edge(s0, s1, s2);          // signed
    if (area == 0) return;                 // degenerate
    float inv_area = 1.0f / area;
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

    for (u32 y = miny; y <= maxy; y++)
    {
        float w0 = row_w0;
        float w1 = row_w1;
        float w2 = row_w2;

        for (u32 x = minx; x <= maxx; x++)
        {
            // this only works is flipped_y == 0
            if (!((w0 < 0 || w1 < 0 || w2 < 0)))
            {
                float b0 = w0 * inv_area;
                float b1 = w1 * inv_area;
                float b2 = w2 * inv_area;
                f32 depth = b0 * v0.z + b1 * v1.z + b2 * v2.z;
                {
                    float inv_w_interp = b0*inv_w0 + b1*inv_w1 + b2*inv_w2;
                    Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(v0_color, b0), vec3_scalar(v1_color, b1)), vec3_scalar(v2_color, b2));
                    Vec3 final_color = vec3_scalar(interpolated_color, 1.0f / inv_w_interp);

                    u32 interpolated_color_to_u32 = 0;

                    interpolated_color_to_u32 |= (0xFF << 24) |
                        (((u32)final_color.x) & 0xFF) << 16 |
                        (((u32)final_color.y) & 0xFF) << 8 |
                        (((u32)final_color.z) & 0xFF) << 0;

                    draw_pixel(buffer, x, y, interpolated_color_to_u32);

                }
            }
            w0 += e0_dx; w1 += e1_dx; w2 += e2_dx; // step right
        }
        row_w0 += e0_dy; row_w1 += e1_dy; row_w2 += e2_dy; // step right
    }
}

internal void barycentric_naive(Params *params)
{
    Vec3 v0 = params->v0;
    Vec3 v1 = params->v1;
    Vec3 v2 = params->v2;
    Vec3 v0_color = params->v0_color;
    Vec3 v1_color = params->v0_color;
    Vec3 v2_color = params->v0_color;
    f32 inv_w0 = params->inv_w0;
    f32 inv_w1 = params->inv_w1;
    f32 inv_w2 = params->inv_w2;
    f32 minx = params->minx;
    f32 maxx = params->maxx;
    f32 miny = params->miny;
    f32 maxy = params->maxy;

    Vec3 plane_normal_of_triangle = vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0));
    f32 area_of_parallelogram = vec3_magnitude(plane_normal_of_triangle);
    f32 area_of_triangle = area_of_parallelogram / 2.0f;
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
            

            Vec3 interpolated_color = vec3_add(vec3_add(vec3_scalar(v0_color, u), vec3_scalar(v1_color, v)), vec3_scalar(v2_color, w));
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

global Obj_Model model_teapot;

internal void render(Params *params)
{
    Obj_Model *model = params->model;
    Mat4 view = params->view;
    Mat4 persp = params->persp;
    if(model->is_valid)
    {
        for(int face_index = 0; face_index < model->face_count; face_index++)
        {
            u32 color = 0xFF00FF11;
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
            
            #if 0
            v0.x += e->position.x;
            v0.y += e->position.y;
            v0.z += e->position.z;
            
            v1.x += e->position.x;
            v1.y += e->position.y;
            v1.z += e->position.z;
            
            v2.x += e->position.x;
            v2.y += e->position.y;
            v2.z += e->position.z;
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
                continue;
            }
            else
            {
                //color = green;
            }



            #if 1
            {
                // remember that the projection matrix stores de viewspace z value in its w
                // but the result vector is in clip space so z is in clip space, not in 
                // viewspace, they are not the same zz
                transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
                transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
                transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
                if(transformed_v0.w != 0)
                {
                    transformed_v0.x /= transformed_v0.w;
                    transformed_v0.y /= transformed_v0.w;
                    transformed_v0.z /= transformed_v0.w;
                }
                
                if(transformed_v1.w != 0)
                {
                    transformed_v1.x /= transformed_v1.w;
                    transformed_v1.y /= transformed_v1.w;
                    transformed_v1.z /= transformed_v1.w;
                }
                
                if(transformed_v2.w != 0)
                {
                    transformed_v2.x /= transformed_v2.w;
                    transformed_v2.y /= transformed_v2.w;
                    transformed_v2.z /= transformed_v2.w;
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
            
            v0.x = (v0.x * 0.5f + 0.5f) * buffer->width;
            v0.y = (v0.y * 0.5f + 0.5f) * buffer->height;

            v1.x = (v1.x * 0.5f + 0.5f) * buffer->width;
            v1.y = (v1.y * 0.5f + 0.5f) * buffer->height;

            v2.x = (v2.x * 0.5f + 0.5f) * buffer->width;
            v2.y = (v2.y * 0.5f + 0.5f) * buffer->height;

            params->v0 = v0;
            params->v1 = v1;
            params->v2 = v2;
            Params tmp = *params;
            barycentric_with_edge_stepping(&tmp);
        }
    }
}


typedef void (Test_Function)(Params *params);

typedef struct Test_Function_Item Test_Function_Item;
struct Test_Function_Item
{
    const char *function_name;
    Test_Function *function; 
};

#define MAX_SAMPLE 4096
typedef struct Test_Function_Result Test_Function_Result;
struct Test_Function_Result
{
    LONGLONG min;
    LONGLONG max;
    LONGLONG avg;
    LONGLONG total;

    LONGLONG samples[MAX_SAMPLE];
    u32 sample_count;

    LONGLONG p50;
    LONGLONG p90;
    LONGLONG p99;
};


typedef struct Tester Tester;
struct Tester
{
    u32 function_count;
    u32 iterations;

    Test_Function_Result *functions_results;
};

int cmp_ll(const void *a, const void *b)
{
    LONGLONG x = *(const LONGLONG*)a;
    LONGLONG y = *(const LONGLONG*)b;
    return (x > y) - (x < y);
}

void compute_percentiles(Test_Function_Result *r)
{
    qsort(r->samples, r->sample_count, sizeof(LONGLONG), cmp_ll);

    #define IDX(p) ((r->sample_count * (p)) / 100)

    r->p50 = r->samples[IDX(50)];
    r->p90 = r->samples[IDX(90)];
    r->p99 = r->samples[IDX(99)];
}

typedef struct SIMD_Result SIMD_Result;
struct SIMD_Result
{
    SIMD_Vec3 *screen_space_vertices;
    f32 *inv_w;
};

typedef struct Scalar_Result Scalar_Result;
struct Scalar_Result
{
    Vec3 *screen_space_vertices;
    f32 *inv_w;
};

internal SIMD_Result stages_separated_simd_output(Params *params)
{
    SIMD_Result result = {0};
    result.screen_space_vertices = (SIMD_Vec3*)malloc(sizeof(SIMD_Vec3));
    result.screen_space_vertices->x = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.screen_space_vertices->y = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.screen_space_vertices->z = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.inv_w = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));

    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    Obj_Model *model = params->model;    
    Obj_Model_SIMD *model_simd = params->model_simd;
    Mat4 view = params->view;
    Mat4 persp = params->persp;

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

    for(u32 vertex_index = 0; vertex_index < model->vertex_count; vertex_index+=4)
    {
        f32 *x_prime = &model_simd->vertices.x[vertex_index];
        f32 *y_prime = &model_simd->vertices.y[vertex_index];
        f32 *z_prime = &model_simd->vertices.z[vertex_index];
        __m128 packed_x_prime = _mm_load_ps(x_prime);
        __m128 packed_y_prime = _mm_load_ps(y_prime);
        __m128 packed_z_prime = _mm_load_ps(z_prime);
        f32 s = 0.2f;
        __m128 scalar = _mm_load_ps1(&s);

        // World space
        packed_x_prime = _mm_mul_ps(packed_x_prime, scalar);
        packed_y_prime = _mm_mul_ps(packed_y_prime, scalar);
        packed_z_prime = _mm_mul_ps(packed_z_prime, scalar);

        __m128 view_space_packed_z = packed_z_prime;
        // perspective matrix
        // after: in clip space
        packed_x_prime = _mm_mul_ps(packed_x_prime, _mm_load_ps1(&g_over_aspect));
        packed_y_prime = _mm_mul_ps(packed_y_prime, _mm_load_ps1(&g));
        packed_z_prime = _mm_add_ps(_mm_mul_ps(packed_z_prime, _mm_load_ps1(&k)), _mm_load_ps1(&minus_znear_times_k));
        #if 0
        // z = view_space_packed_z
        const __m128 sign_mask = _mm_set1_ps(-0.0f);
        const __m128 abs_mask  = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
        const __m128 eps       = _mm_set1_ps(1e-8f);

        __m128 z      = view_space_packed_z;
        __m128 sign   = _mm_and_ps(z, sign_mask);        // +/- sign bit
        __m128 abs_z  = _mm_and_ps(z, abs_mask);         // |z|
        __m128 abs_cl = _mm_max_ps(abs_z, eps);          // clamp abs(z) >= eps
        __m128 safe_z = _mm_or_ps(abs_cl, sign);         // restore sign

        __m128 inv_w  = _mm_div_ps(_mm_set1_ps(1.0f), safe_z);

        packed_x_prime = _mm_mul_ps(packed_x_prime, inv_w);
        packed_y_prime = _mm_mul_ps(packed_y_prime, inv_w);
        packed_z_prime = _mm_mul_ps(packed_z_prime, inv_w);
        #else
        __m128 one    = _mm_set1_ps(1.0f);
        __m128 zero   = _mm_set1_ps(0.0f);

        // mask = 0xFFFFFFFF where w != 0, else 0
        __m128 mask = _mm_cmpneq_ps(view_space_packed_z, zero);

        // Numerator masked → 1.0 only in lanes where we will divide
        __m128 num = _mm_and_ps(one, mask);

        // Denominator masked → 0 in lanes where w == 0
        __m128 den = _mm_and_ps(view_space_packed_z, mask);

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

        // perspective divide
        // after: in NDC space
        // how do i handle division by zero without branching?
        #endif

        // viewport space
        // this: _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
        // should be equivalent to: v.x = (v.x * 0.5f + 0.5f) * buffer->width;
        // after reordering: v.x = v.x * buffer->width / 2 + buffer->width / 2
        // or: v.x = v.x * half_width + half_width
        // same for v.y

        packed_x_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
        packed_y_prime = _mm_add_ps(_mm_mul_ps(packed_y_prime, half_height), half_height);
        
        _mm_store_ps(result.screen_space_vertices->x + vertex_index, packed_x_prime);
        _mm_store_ps(result.screen_space_vertices->y + vertex_index, packed_y_prime); 
        _mm_store_ps(result.screen_space_vertices->z + vertex_index, packed_z_prime); 
        //_mm_store_ps(result.inv_w + vertex_index, packed_inv_w); 
    }

    return result;
}

global Vec3 *screen_space_vertices;
global f32 *inv_w;
global SIMD_Vec3 *screen_space_vertices_simd;
global f32 *inv_w_simd;

internal b32 compare_results(SIMD_Result *simd_result, Scalar_Result *scalar_result, u32 vertex_count)
{
    b32 result = 1;
    u32 mismatch_count = 0;
    for(u32 vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        f32 simd_x = simd_result->screen_space_vertices->x[vertex_index];
        f32 scalar_x = scalar_result->screen_space_vertices[vertex_index].x;
        f32 simd_y = simd_result->screen_space_vertices->y[vertex_index];
        f32 scalar_y = scalar_result->screen_space_vertices[vertex_index].y;
        f32 simd_z = simd_result->screen_space_vertices->z[vertex_index];
        f32 scalar_z = scalar_result->screen_space_vertices[vertex_index].z;
        if(!equal_f32(simd_x, scalar_x) ||
           !equal_f32(simd_y, scalar_y) ||
           !equal_f32(simd_z, scalar_z) ) 
        {
            printf("Vertex %u mismatch:\n", vertex_index);
            printf("  SIMD   = (%.9g, %.9g, %.9g)\n", simd_x, simd_y, simd_z);
            printf("  Scalar = (%.9g, %.9g, %.9g)\n", scalar_x, scalar_y, scalar_z);
            printf("  diff   = (%.9g, %.9g, %.9g)\n",
            simd_x - scalar_x, simd_y - scalar_y, simd_z - scalar_z);
            mismatch_count++;
            result = 0;
        }

    }
    printf("%d mismatches out of %d vertices \n", mismatch_count, vertex_count);
    return result;
};

internal void stages_separated_simd(Params *params)
{
    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    Obj_Model *model = params->model;    
    Obj_Model_SIMD *model_simd = params->model_simd;
    SIMD_Result result = {0};
    result.screen_space_vertices = (SIMD_Vec3*)malloc(sizeof(SIMD_Vec3));
    result.screen_space_vertices->x = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.screen_space_vertices->y = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.screen_space_vertices->z = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    result.inv_w = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));
    Mat4 view = params->view;
    Mat4 persp = params->persp;

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

    __m128 half_width = _mm_mul_ps(_mm_load_ps1(&(f32)buffer->width), _mm_load_ps1(&half));
    __m128 half_height = _mm_mul_ps(_mm_load_ps1(&(f32)buffer->height), _mm_load_ps1(&half));
    for(u32 vertex_index = 0; vertex_index < model->vertex_count; vertex_index+=4)
    {
        f32 *x_prime = &model_simd->vertices.x[vertex_index];
        f32 *y_prime = &model_simd->vertices.y[vertex_index];
        f32 *z_prime = &model_simd->vertices.z[vertex_index];
        __m128 packed_x_prime = _mm_load_ps(x_prime);
        __m128 packed_y_prime = _mm_load_ps(y_prime);
        __m128 packed_z_prime = _mm_load_ps(z_prime);
        f32 s = 0.2f;
        __m128 scalar = _mm_load_ps1(&s);

        // World space
        packed_x_prime = _mm_mul_ps(packed_x_prime, scalar);
        packed_y_prime = _mm_mul_ps(packed_y_prime, scalar);
        packed_z_prime = _mm_mul_ps(packed_z_prime, scalar);

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

        // Denominator masked → 0 in lanes where w == 0
        __m128 den = _mm_and_ps(view_space_packed_z, mask);

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


        // viewport space
        // this: _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
        // should be equivalent to: v.x = (v.x * 0.5f + 0.5f) * buffer->width;
        // after reordering: v.x = v.x * buffer->width / 2 + buffer->width / 2
        // or: v.x = v.x * half_width + half_width
        // same for v.y

        packed_x_prime = _mm_add_ps(_mm_mul_ps(packed_x_prime, half_width), half_width);
        packed_y_prime = _mm_add_ps(_mm_mul_ps(packed_y_prime, half_height), half_height);
        
        _mm_store_ps(result.screen_space_vertices->x + vertex_index, packed_x_prime);
        _mm_store_ps(result.screen_space_vertices->y + vertex_index, packed_y_prime); 
        _mm_store_ps(result.screen_space_vertices->z + vertex_index, packed_z_prime); 
        _mm_store_ps(result.inv_w + vertex_index, inv_w); 
    }


    for(int face_index = 0; face_index < model->face_count; face_index++)
    {
        {
            Face face = model->faces[face_index];
            Vec3 v0 = (Vec3) {result.screen_space_vertices->x[face.v[0] - 1], result.screen_space_vertices->y[face.v[0] - 1], result.screen_space_vertices->z[face.v[0] - 1]};
            Vec3 v1 = (Vec3) {result.screen_space_vertices->x[face.v[1] - 1], result.screen_space_vertices->y[face.v[1] - 1], result.screen_space_vertices->z[face.v[1] - 1]};
            Vec3 v2 = (Vec3) {result.screen_space_vertices->x[face.v[2] - 1], result.screen_space_vertices->y[face.v[2] - 1], result.screen_space_vertices->z[face.v[2] - 1]};

            f32 inv_w0 = result.inv_w[face.v[0] - 1];
            f32 inv_w1 = result.inv_w[face.v[1] - 1];
            f32 inv_w2 = result.inv_w[face.v[2] - 1];

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

            barycentric_with_edge_stepping(&params);
        }
    }
}

internal Scalar_Result stages_separated_scalar_output(Params *params)
{
    Scalar_Result result = {0};
    result.screen_space_vertices = (Vec3*)malloc(sizeof(Vec3) * (params->model->vertex_count));
    result.inv_w = (f32*)malloc(sizeof(f32) * (params->model->vertex_count));

    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    Obj_Model *model = params->model;    
    Mat4 view = params->view;
    Mat4 persp = params->persp;


    for(u32 vertex_index = 0; vertex_index < model->vertex_count; vertex_index++)
    {
        Vec3 v = model->vertices[vertex_index];
        
        // World space
        v = vec3_scalar(v, 0.2f);
        
        // view space
        Vec4 transformed_v0 = (Vec4){.x = v.x, .y = v.y, .z = v.z, .w = 1};

        f32 inv_w0;
        transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
        #if 1
        if(transformed_v0.w != 0)
        {
            inv_w0 = 1.0f / transformed_v0.w;
            transformed_v0.x *= inv_w0;
            transformed_v0.y *= inv_w0;
            transformed_v0.z *= inv_w0;
        }
        #endif
        


        v.x = transformed_v0.x;
        v.y = transformed_v0.y;
        v.z = transformed_v0.z;


        v.x = (v.x * 0.5f + 0.5f) * buffer->width;
        v.y = ((v.y * 0.5f + 0.5f)) * buffer->height;

        result.screen_space_vertices[vertex_index] = v;
        //result.inv_w[vertex_index] = inv_w0;
    }


    return result;
}

internal void stages_separated(Params *params)
{
    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    Obj_Model *model = params->model;    
    Mat4 view = params->view;
    Mat4 persp = params->persp;


    for(u32 vertex_index = 0; vertex_index < model->vertex_count; vertex_index++)
    {
        Vec3 v = model->vertices[vertex_index];
        
        // World space
        v = vec3_scalar(v, 0.2f);
        
        // view space
        Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v.x, .y = v.y, .z = v.z, .w = 1});
        Vec3 transformed_v0_v3 = (Vec3) {transformed_v0.x, transformed_v0.y, transformed_v0.z};

        f32 inv_w0;
        transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
        //BeginTime("transformation", 0);
        if(transformed_v0.w != 0)
        {
            inv_w0 = 1.0f / transformed_v0.w;
            transformed_v0.x *= inv_w0;
            transformed_v0.y *= inv_w0;
            transformed_v0.z *= inv_w0;
        }
        

        v.x = transformed_v0.x;
        v.y = transformed_v0.y;
        v.z = transformed_v0.z;


        v.x = (v.x * 0.5f + 0.5f) * buffer->width;
        #if FLIPPED_Y
        v.y = (1.0f - (v.y * 0.5f + 0.5f)) * buffer->height;
        #else
        v.y = ((v.y * 0.5f + 0.5f)) * buffer->height;
        #endif

        screen_space_vertices[vertex_index] = v;
        inv_w[vertex_index] = inv_w0;
    }


    for(int face_index = 0; face_index < model->face_count; face_index++)
    {
        {
            Face face = model->faces[face_index];
            Vec3 v0 = screen_space_vertices[face.v[0]];
            Vec3 v1 = screen_space_vertices[face.v[1]];
            Vec3 v2 = screen_space_vertices[face.v[2]];

            f32 inv_w0 = inv_w[face.v[0]];
            f32 inv_w1 = inv_w[face.v[1]];
            f32 inv_w2 = inv_w[face.v[2]];

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

            barycentric_with_edge_stepping(&params);
        }
    }
}

internal void stages_combined(Params *params)
{
    Vec3 vv0_color = (Vec3) {255, 0, 0};
    Vec3 vv1_color = (Vec3) {0, 255, 0};
    Vec3 vv2_color = (Vec3) {0, 0, 255};
    Obj_Model *model = params->model;    
    Mat4 view = params->view;
    Mat4 persp = params->persp;

    for(int face_index = 0; face_index < model->face_count; face_index++)
    {
        {
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
            #if FLIPPED_Y
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

            /////draw_triangle__scanline(buffer, v0, v1, v2, color);
            Params params = 
            {
                view, persp,
                v0, v1, v2,
                new_vv0_color, new_vv1_color, new_vv2_color,
                inv_w0, inv_w1, inv_w2,
                min_x, max_x, min_y, max_y
            };

            barycentric_with_edge_stepping(&params);
        }
    }

}

internal Obj_Model_SIMD obj_to_simd(Obj_Model model)
{
    Obj_Model_SIMD result = {0};
    u32 count = model.vertex_count;
    if (model.vertex_count % 4 != 0)
    {
        count = (u32)roundf((f32)model.vertex_count / 4.0f) * 4.0f;
        printf("Rounding %d to %d\n", model.vertex_count, count);
    }

    result.vertices.x = (f32*) malloc(sizeof(f32) * (count));
    result.vertices.y = (f32*) malloc(sizeof(f32) * (count));
    result.vertices.z = (f32*) malloc(sizeof(f32) * (count));
    for(u32 vertex_index = 0; vertex_index < model.vertex_count; vertex_index++)
    {
        Vec3 model_vertex = model.vertices[vertex_index];
        result.vertices.x[vertex_index] = model_vertex.x;
        result.vertices.y[vertex_index] = model_vertex.y;
        result.vertices.z[vertex_index] = model_vertex.z;
    }

    return result;
}


int main()
{
    timer_init();
    const char *filename = ".\\obj\\teapot.obj";
    OS_FileReadResult obj = os_file_read(filename);
    model_teapot = parse_obj(obj.data, obj.size);
    Obj_Model_SIMD model_simd = obj_to_simd(model_teapot);

    printf("Loaded: %s, triangle count: %d\n", filename, model_teapot.face_count );
    time_context = VirtualAlloc(0, sizeof(TimeContext), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    i32 buffer_width = 640;
    i32 buffer_height = 480;
    buffer = VirtualAlloc(0, sizeof(Software_Render_Buffer), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    buffer->width = buffer_width;
    buffer->height = buffer_height;
    buffer->data = VirtualAlloc(0, sizeof(u32) * buffer->width * buffer->height, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    buffer->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = sizeof(u32) * 8;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	buffer->info.bmiHeader.biSizeImage = 0;
	buffer->info.bmiHeader.biXPelsPerMeter = 0;
	buffer->info.bmiHeader.biYPelsPerMeter = 0;
	buffer->info.bmiHeader.biClrUsed = 0;
	buffer->info.bmiHeader.biClrImportant = 0;

    Mat4 view = mat4_identity();
    f32 fov = 3.141592 / 3.0; // 60 deg
    f32 aspect = (f32)buffer->width / (f32)buffer->height;
    f32 znear = 1.0f;
    f32 zfar = 50.0f;
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);

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

    f32 inv_w0 = 0;
    f32 inv_w1 = 0;
    f32 inv_w2 = 0;
    #if 1
    {
        Params params = {0};

        params.view = view;
        params.persp = persp;
        params.v0 = vec3_from_vec4(v0_v4);
        params.v1 = vec3_from_vec4(v1_v4);
        params.v2 = vec3_from_vec4(v2_v4);
        params.inv_w0 = inv_w0;
        params.inv_w1 = inv_w1;
        params.inv_w2 = inv_w2;
        create_and_apply_mvp(&params);
        v0_v4 = vec4_from_vec3(params.v0);
        v1_v4 = vec4_from_vec3(params.v1);
        v2_v4 = vec4_from_vec3(params.v2);
        inv_w0 = params.inv_w0;
        inv_w1 = params.inv_w1;
        inv_w2 = params.inv_w2;
    }


    #else
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



    Test_Function_Item function_table[] = 
    {

        {"[SIMD] stage 1 and stage 2", stages_separated_simd},
        {"stage 1 and stage 2", stages_separated},
        {"stages combined", stages_combined},
        #if 0
        {"[SCALAR] render model using barycentric with edge stepping", render},
        {"[SCALAR] mvp creation and applying", create_and_apply_mvp},
        {"[SCALAR] barycentric with edge stepping", barycentric_with_edge_stepping},
        {"[SCALAR] barycentric_naive", barycentric_naive},
        {"[SCALAR] olivec_version", olivec_version},
        #endif
    };

    Tester tester = {0};

    int function_count  = ArrayCount(function_table);
    tester.function_count = function_count;
    tester.functions_results = VirtualAlloc(0, tester.function_count * sizeof(Test_Function_Result), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    tester.iterations = 1;
    for(u32 i = 0; i < tester.function_count; i++)
    {
        tester.functions_results[i].max = 0;
        tester.functions_results[i].min = 999999999999;
    }

    Params params = {view, persp, v0, v1, v2, vv0_color, vv1_color, vv2_color, inv_w0, inv_w1, inv_w2, minx, maxx, miny, maxy};
    params.model = &model_teapot;
    assert(params.model->vertex_count % 4 == 0);
    params.model_simd = &model_simd;
    SIMD_Result simd_result = stages_separated_simd_output(&params);
    Scalar_Result scalar_result = stages_separated_scalar_output(&params);
    if(!compare_results(&simd_result, &scalar_result, params.model->vertex_count))
    {
        printf("Math is not right!\n");
        return -1;
    }

    screen_space_vertices = (Vec3*)malloc(sizeof(Vec3) * (params.model->vertex_count));
    inv_w = (f32*)malloc(sizeof(f32) * (params.model->vertex_count));
    screen_space_vertices_simd = (SIMD_Vec3*)malloc(sizeof(SIMD_Vec3));
    screen_space_vertices_simd->x = (f32*)malloc(sizeof(f32) * (params.model->vertex_count));
    screen_space_vertices_simd->y = (f32*)malloc(sizeof(f32) * (params.model->vertex_count));
    screen_space_vertices_simd->z = (f32*)malloc(sizeof(f32) * (params.model->vertex_count));
    inv_w_simd = (f32*)malloc(sizeof(f32) * (params.model->vertex_count));

    for(u32 function_index = 0; function_index < function_count; function_index++)
    {
        Test_Function_Item *function_metadata = function_table + function_index;
        Test_Function_Result *result = tester.functions_results + function_index;
        for (u32 i = 0; i < tester.iterations; i++)
        {
            LONGLONG start = timer_get_os_time();
            Params tmp = params;
            function_metadata->function(&tmp);

            LONGLONG end = timer_get_os_time();
            LONGLONG final_time = end - start;

            result->min = Min(tester.functions_results[function_index].min, final_time);
            result->max = Max(tester.functions_results[function_index].max, final_time);
            result->total += final_time;

            result->samples[result->sample_count++] = final_time;
        }
        compute_percentiles(result);
        printf("%s: \n", function_metadata->function_name);
        printf("min: %.5fms\n",  timer_os_time_to_ms(result->min));
        printf("max: %.5fms\n",  timer_os_time_to_ms(result->max));
        printf("avg: %.5fms\n",  timer_os_time_to_ms(result->total / tester.iterations));

        printf("p50: %.5fms\n",  timer_os_time_to_ms(result->p50));
        printf("p90: %.5fms\n",  timer_os_time_to_ms(result->p90));
        printf("p99: %.5fms\n",  timer_os_time_to_ms(result->p99));
        //printf("total: %.5fms\n", timer_os_time_to_ms(tester.functions_results[function_index].total));
        printf("------------------------------------ \n\n");
    }
    // try first on game if this makes a difference which will probably certainly almost will
    free(screen_space_vertices);
    free(inv_w);


    #if 0
    for (u32 i = 0; i < 20; i++)
    {
        if (anchors[i].result == 0) continue;
        printf("[%s] hit per frame: %d, time_total: %.5fms  time_average: %.5fms\n", anchors[i].name, anchors[i].count, timer_os_time_to_ms(anchors[i].result), timer_os_time_to_ms(anchors[i].result) / anchors[i].count);
        anchors[i].result = 0;
        anchors[i].count = 0;
    }
    #endif


}