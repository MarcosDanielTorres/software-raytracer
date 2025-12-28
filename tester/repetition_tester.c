#include "base_core.h"
#include "base_os.h"
#include "base_math.h"
#include "os_win32.h"
#include "timer.h"
#include "timer.c"
#define EDGE_FUNCTIONS 0
#define EDGE_STEPPING 0

global Software_Render_Buffer *buffer;

typedef struct Params Params;
struct Params
{
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
};
typedef void (Test_Function)(Params *params);

typedef struct Test_Function_Item Test_Function_Item;
struct Test_Function_Item
{
    const char *function_name;
    Test_Function *function; 
};

typedef struct Test_Function_Result Test_Function_Result;
struct Test_Function_Result
{
    LONGLONG min;
    LONGLONG max;
    LONGLONG avg;
    LONGLONG total;
};

typedef struct Tester Tester;
struct Tester
{
    u32 function_count;
    u32 iterations;
    Test_Function_Result functions_results[100];
};


internal void create_and_apply_mvp(Params *params)
{
    Vec4 v0 = params->v0;
    Vec4 *v1 = params->v1;
    Vec4 *v2 = params->v2;
    f32 *inv_w0 = params->inv_w0;
    f32 *inv_w1 =  params->inv_w1;
    f32 *inv_w2 = params->inv_w2;

    Mat4 view = mat4_identity();
    f32 fov = 3.141592 / 3.0; // 60 deg
    f32 aspect = (f32)buffer->width / (f32)buffer->height;
    f32 znear = 1.0f;
    f32 zfar = 50.0f;
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);

    *v0 = mat4_mul_vec4(view, *v0);
    *v1 = mat4_mul_vec4(view, *v1);
    *v2 = mat4_mul_vec4(view, *v2);

    *v0 = mat4_mul_vec4(persp, *v0);
    *v1 = mat4_mul_vec4(persp, *v1);
    *v2 = mat4_mul_vec4(persp, *v2);

    *inv_w0 = 1.0f / v0->w;
    *inv_w1 = 1.0f / v1->w;
    *inv_w2 = 1.0f / v2->w;
    if(v0->w != 0)
    {
        v0->x *= *inv_w0;
        v0->y *= *inv_w0;
        v0->z *= *inv_w0;
    }
    
    if(v1->w != 0)
    {
        v1->x *= *inv_w1;
        v1->y *= *inv_w1;
        v1->z *= *inv_w1;
    }
    
    if(v2->w != 0)
    {
        v2->x *= *inv_w2;
        v2->y *= *inv_w2;
        v2->z *= *inv_w2;
    }

    v0->x = (v0->x * 0.5f + 0.5f) * buffer->width;
    v0->y = (v0->y * 0.5f + 0.5f) * buffer->height;

    v1->x = (v1->x * 0.5f + 0.5f) * buffer->width;
    v1->y = (v1->y * 0.5f + 0.5f) * buffer->height;

    v2->x = (v2->x * 0.5f + 0.5f) * buffer->width;
    v2->y = (v2->y * 0.5f + 0.5f) * buffer->height;
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

int main()
{

    timer_init();
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

        params.v0 = v0_v4;
        params.v1 = v1_v4;
        params.v2 = v2_v4;
        params.inv_w0 = inv_w0;
        params.inv_w1 = inv_w1;
        params.inv_w2 = inv_w2;
        create_and_apply_mvp(&params);
        v0_v4 = params.v0;
        v1_v4 = params.v1;
        v2_v4 = params.v2;
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
        {"[SCALAR] mvp creation and applying", create_and_apply_mvp},
        {"[SCALAR] barycentric with edge stepping", barycentric_with_edge_stepping},
        {"[SCALAR] barycentric_naive", barycentric_naive},
        {"[SCALAR] olivec_version", olivec_version},
    };

    Tester tester = {0};

    int function_count  = ArrayCount(function_table);
    tester.function_count = function_count;
    tester.iterations = 1000;
    for(u32 i = 0; i < tester.function_count; i++)
    {
        tester.functions_results[i].max = 0;
        tester.functions_results[i].min = 999999999999;
    }

    Params params = {v0, v1, v2, vv0_color, vv1_color, vv2_color, inv_w0, inv_w1, inv_w2, minx, maxx, miny, maxy};
    for(u32 function_index = 0; function_index < function_count; function_index++)
    {
        Test_Function_Item *function_metadata = function_table + function_index;
        for (u32 i = 0; i <= tester.iterations; i++)
        {
            LONGLONG start = timer_get_os_time();

            function_metadata->function(&params);

            LONGLONG end = timer_get_os_time();
            LONGLONG final_time = end - start;

            tester.functions_results[function_index].min = Min(tester.functions_results[function_index].min, final_time);
            tester.functions_results[function_index].max = Max(tester.functions_results[function_index].max, final_time);
            tester.functions_results[function_index].total += final_time;
        }
        printf("%s: \n", function_metadata->function_name);
        printf("min: %.5fms\n",  timer_os_time_to_ms(tester.functions_results[function_index].min));
        printf("max: %.5fms\n",  timer_os_time_to_ms(tester.functions_results[function_index].max));
        printf("avg: %.5fms\n",  timer_os_time_to_ms(tester.functions_results[function_index].total / tester.iterations));
        //printf("total: %.5fms\n", timer_os_time_to_ms(tester.functions_results[function_index].total));
        printf("------------------------------------ \n\n");
    }


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