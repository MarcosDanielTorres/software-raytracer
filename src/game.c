#include "base_core.h"
#include "base_os.h"
#include "os_win32.h"
#include "timer.h"
#include "os_win32.c"
#include "base_math.h"
#include "timer.c"
#include <assert.h>


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
    Vec3 *points;
    int points_count;
    Face *faces;
    int faces_count;
    b32 is_valid;
    b32 has_normals;
};

inline b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
}

Obj_Model parse_obj(char *buf, size_t size)
{
    LONGLONG start = timer_get_os_time();
    //aim_profiler_time_function;
    Obj_Model model = {0};
    if(size == 0)
    {
        return model;
    }
    model.points = (Vec3*)malloc(sizeof(Vec3) * 9000);
    model.faces = (Face*)malloc(sizeof(Face) * 9000);
    char* at = buf;
    while (*at == ' ' || *at == '\r' || *at == '\n')
    {
        at++;
    }
    
    while (*at != '\0')
    {
		switch (*at)
		{
			case 'v': {
				at++;
                switch(*at)
                {
                    case ' ':
                    {
                        f32 coords[3];
                        f32 coords2[3];
                        int i = 0;
                        while(i < 3)
                        {
                            at++;
                            char *start = at;
#if 0
                            f32 a = 0;
                            b32 found = 0;
                            int past_point = 0;
                            int inc = 1;
                            b32 scientific_notation = 0;
                            f32 sign = 1;
                            if (*at == '-')
                            {
                                sign = -1;
                            }
                            while (*at != '\n' && *at != '\0' && *at != ' ')
                            {
                                if (is_digit(*at))
                                {
                                    a *= 10.0f;
                                    a += (f32)(*at - '0');
                                    if (found)
                                        inc *= 10;
                                    
                                }
                                if(*at == 'e' || *at == 'E')
                                {
                                    scientific_notation = 1;
                                    break;
                                }
                                if (*at == '.')
                                {
                                    found = 1;
                                }
                                at++;
                            }
                            if (found)
                            {
                                a = a / inc;
                            }
                            a *= sign;

                            if (scientific_notation)
                            {
                                at++;
                                // assuming + is 0
                                b32 scientific_notation_sign = 0;
                                if(*at == '-')
                                {
                                    scientific_notation_sign = 1;
                                    at++;
                                }
                                else if(*at == '+')
                                {
                                    at++;
                                }

                                while(*at == '0')
                                {
                                    at++;
                                }
                                u32 exponent = 0;
                                while(*at != '\n' && *at != ' ' && *at != '\0')
                                {
                                    if(is_digit(*at))
                                    {
                                       exponent *= 10;
                                       exponent += (u32)(*at - '0');
                                    }
                                    at++;
                                }
                                u32 divisor = 1;
                                for(u32 i = 0; i < exponent; i++)
                                {
                                   divisor *= 10; 
                                }

                                if(scientific_notation_sign == 0)
                                {
                                    a *= divisor;
                                }
                                else
                                {
                                    a /= divisor;
                                }
                            }
                            coords[i] = a;
#else
                            
                            
                            f32 num;
                            char *pEnd;
                            num = strtof(start, &pEnd);
                            at = pEnd;
                            //coords2[i] = num;
                            coords[i] = num;
#endif
                            int x = 321;
                            //sscanf(start, "%f", &num);
                            i++;
                        }
                        at++;
                        model.points_count++;
                        model.points[model.points_count] = (Vec3){coords[0], coords[1], coords[2]};
                        //printf("v %.7f %.7f %.7f\n", coords[0], coords[1], coords[2]);
                        //printf("v %.7f %.7f %.7f\n", coords2[0], coords2[1], coords2[2]);
                    }break;
                    default:
                    {
                        at++;
                    }
			    } break;
            }
			case 'f': {
				at+=2;
				Face face;
                int* v = face.v;
				int* vt = face.vt;
				int* vn = face.vn;
                
				int j = 0;
				while (j < 3)
				{
                    int i = 0;
                    int attrib_counter = 0;
                    int index = 0;
                    while (*at != '\0')
                    {
                        if(*at == ' ' || *at == '\n' || *at == '\r')
                        {
                            if (*at == '\r') at ++;
                            if (attrib_counter == 0)
                            {
                                *v++ = index;
                                index = 0;
                            }
                            else if (attrib_counter == 1)
                            {
                                *vt++ = index;
                                index = 0;
                            }
                            else if (attrib_counter == 2)
                            {
                                *vn++ = index;
                                index = 0;
                            }
                            attrib_counter++;
                            break;
                        }
                        if(*at == '/' )
                        {
                            if (attrib_counter == 0)
                            {
                                *v++ = index;

                                index = 0;

                            }
                            else if (attrib_counter == 1)
                            {
                                *vt++ = index;
                                index = 0;
                            }
                            attrib_counter++;
                            at++;
                            continue;
                        }

                        if (is_digit(*at))
                        {
                            index *= 10;
                            index += (int)(*at - '0');
                        }
                        at++;
                    }

                    
                    face.packed_data[j * 3 + i] = index;
                    at++;
					j++;
				}
				model.faces_count++;
				model.faces[model.faces_count] = face;
				//printf("f %d/%d/%d %d/%d/%d %d/%d/%d\n", face.packed_data[0],face.packed_data[1],face.packed_data[2],face.packed_data[3],face.packed_data[4],face.packed_data[5], face.packed_data[6], face.packed_data[7], face.packed_data[8]);
				//printf("f %d/%d/%d %d/%d/%d %d/%d/%d\n", face.v[0],face.vt[0],face.vn[0],face.v[1],face.vt[1],face.vn[1], face.v[2], face.vt[2], face.vn[2]);
			} break;
			case '#':
			{
                char *start = at;
                while(*at != '\n' && *at != '\0')
                {
                    at++;
                }
                //printf("%.*s\n", size, start);
				at++;
			} break;
			default:
			{
				//printf("Unrecognized symbol %c\n", *at);
				at++;
			}
		}
    }
    
    LONGLONG end = timer_get_os_time();
    LONGLONG result = end - start;
    printf("parse_obj: %.2fms\n", timer_os_time_to_ms(result));
    model.is_valid = 1;
    return model;
};

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

internal void draw_pixel(Software_Render_Buffer *buffer, f32 x, f32 y, u32 color)
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

//Vec3 point_rotate_y(Vec3 p, f32 angle)
Vec3 point_scalar(Vec3 p, f32 s)
{
    return (Vec3) {p.x * s, p.y * s, p.z * s};
}
Vec3 point_rotate_y(Vec3 p, f32 c, f32 s)
{
    Vec3 result = {0};
    
    //f32 c = cos(angle);
    //f32 s = sin(angle);
    
    result.x = p.x * c + p.z * s;
    result.y = p.y;
    result.z = -p.x * s + p.z * c;
    
    //result.x = p.x;
    //result.y = p.y * c - p.z * s;
    //result.z = p.y * s + p.z * c;
    
    
    return result;
}

Vec3 point_rotate_x(Vec3 p, f32 c, f32 s)
{
    Vec3 result = {0};
    
    //f32 c = cos(angle);
    //f32 s = sin(angle);
    
    result.x = p.x;
    result.z = p.z * c + p.x * s;
    result.y = -p.z * s + p.y * c;
    
    //result.x = p.x;
    //result.y = p.y * c - p.z * s;
    //result.z = p.y * s + p.z * c;
    
    
    return result;
}

Vec3 point_rotate_z(Vec3 p, f32 c, f32 s)
{
    Vec3 result = {0};
    
    //f32 c = cos(angle);
    //f32 s = sin(angle);
    
    result.x = p.x * c + p.y * s;
    result.y = -p.x * s + p.y * c;
    result.z = p.z;
    
    //result.x = p.x;
    //result.y = p.y * c - p.z * s;
    //result.z = p.y * s + p.z * c;
    
    
    return result;
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
        printf("Loaded: %s, triangle count: %d\n", filename, model_african_head.faces_count / 3);

        filename = ".\\obj\\teapot.obj";
        obj = os_file_read(filename);
        model_teapot = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_teapot.faces_count / 3);

        filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
        obj = os_file_read(filename);
        model_diablo = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_diablo.faces_count / 3);

        filename = ".\\obj\\f117.obj";
        obj = os_file_read(filename);
        model_f117 = parse_obj(obj.data, obj.size);
        printf("Loaded: %s, triangle count: %d\n", filename, model_f117.faces_count / 3);
        
        {
            entities[entity_count++] = (Entity) { .name = "enemy_1", .model = &model_teapot, .position = (Vec3) {0.0, -0.1, 3} };
            entities[entity_count++] = (Entity) { .name = "enemy_2", .model = &model_f117, .position = (Vec3) {-0.8, 0.3, 2} };
            entities[entity_count++] = (Entity) { .name = "enemy_3", .model = &model_diablo, .position = (Vec3) {0.5, 0, 1} };
            entities[entity_count++] = (Entity) { .name = "enemy_4", .model = &model_african_head, .position = (Vec3) {-0.5, -0.2, 1} };
            // should in theory??? be contained between +- 2.303627
        }
        printf("Entity count: %d\n", entity_count);

        time_context = (TimeContext*)malloc(sizeof(TimeContext));
        init = 1;
    }
    u32 black = 0xff000000;
    u32 white = 0xffffffff;
    u32 red = 0xffff0000;
    u32 green = 0xff00ff00;
    u32 blue = 0xff0000ff;
    u32 steam_chat_background_color = 0xff1e2025;
    //draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, 0xff111f0f);
    draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, steam_chat_background_color);
    
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
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);

    // rolled perspective
    f32 g = 1.0f / tan(fov * 0.5f);
    f32 g_over_aspect = g / aspect;
    // rolled perspective

    Mat4 view = mat4_look_at(camera_eye, camera_target, (Vec3) {0, 1, 0});
    view = mat4_identity();
    
    LONGLONG model_now = timer_get_os_time();
	f32 c = cos(angle);
	f32 s = sin(angle);
	f32 c_90 = cos(3.14 / 2.0f);
	f32 s_90 = sin(3.14 / 2.0f);
    {
        BeginTime("rendering models", 1);
        for (u32 entity = 0; entity < entity_count; entity++)
        //for (u32 entity = 0; entity < 1; entity++)
        {
            Entity* e = entities + entity;
            Obj_Model *model = e->model;
            if(model->is_valid)
            {
                for(int face_index = 1; face_index <= model->faces_count; face_index++)
                {
                    u32 color = green;
                    Face face = model->faces[face_index];
                    Vec3 v0 = model->points[face.v[0]];
                    Vec3 v1 = model->points[face.v[1]];
                    Vec3 v2 = model->points[face.v[2]];

                    if (model->has_normals)
                    {
						Vec3 n0 = model->points[face.vn[0]];
						Vec3 n1 = model->points[face.vn[1]];
						Vec3 n2 = model->points[face.vn[2]];
                    }


                    if(model == &model_f117)
                    {

                        v0 = point_rotate_z(v0, c_90, s_90);
                        v1 = point_rotate_z(v1, c_90, s_90);
                        v2 = point_rotate_z(v2, c_90, s_90);

                        v0 = point_rotate_y(v0, c, s);
                        v1 = point_rotate_y(v1, c, s);
                        v2 = point_rotate_y(v2, c, s);



                    }
                    else
                    {
						v0 = point_rotate_y(v0, c, s);
						v1 = point_rotate_y(v1, c, s);
						v2 = point_rotate_y(v2, c, s);
                    }
                    
                    // World space
                    v0 = point_scalar(v0, 0.2f);
                    v1 = point_scalar(v1, 0.2f);
                    v2 = point_scalar(v2, 0.2f);
                    
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
                        color = blue;
                        continue;
                    }
                    else
                    {
                        color = green;
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
                    
                    #if 1
                    {
                        transformed_v0 = mat4_mul_vec4(persp, transformed_v0);
                        transformed_v1 = mat4_mul_vec4(persp, transformed_v1);
                        transformed_v2 = mat4_mul_vec4(persp, transformed_v2);
                        //BeginTime("transformation", 0);
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
                    //EndTime();
                    
                    f32 mapped_v0_x;
                    f32 mapped_v0_y;
                    f32 mapped_v1_x;
                    f32 mapped_v1_y;
                    f32 mapped_v2_x;
                    f32 mapped_v2_y;
                    
                    //BeginTime("hello", 0);
                    {

                        #if 1
                        mapped_v0_x = (transformed_v0.x * 0.5f + 0.5f) * buffer->width;
                        mapped_v0_y = (1.0f - (transformed_v0.y * 0.5f + 0.5f)) * buffer->height;
                        mapped_v1_x = (transformed_v1.x * 0.5f + 0.5f) * buffer->width;
                        mapped_v1_y = (1.0f - (transformed_v1.y * 0.5f + 0.5f)) * buffer->height;
                        mapped_v2_x = (transformed_v2.x * 0.5f + 0.5f) * buffer->width;
                        mapped_v2_y = (1.0f - (transformed_v2.y * 0.5f + 0.5f)) * buffer->height;



                        #else
						mapped_v0_x = map_range(transformed_v0.x, -1.0f, 1.0f, 0.0f, buffer->width);
						mapped_v0_y = map_range(transformed_v0.y, -1.0f, 1.0f, buffer->height, 0.0f);
						mapped_v1_x = map_range(transformed_v1.x, -1.0f, 1.0f, 0.0f, buffer->width);
						mapped_v1_y = map_range(transformed_v1.y, -1.0f, 1.0f, buffer->height, 0.0f);
						mapped_v2_x = map_range(transformed_v2.x, -1.0f, 1.0f, 0.0f, buffer->width);
						mapped_v2_y = map_range(transformed_v2.y, -1.0f, 1.0f, buffer->height, 0.0f);
                        #endif
                    }
                    //EndTime();
                    //printf("First coord mapped %.4f -> %.4f\n", v0.x, mapped_v0_x);
                    //BeginTime("dsa", 0);
                    {
                        Vec2F32 v0 = {mapped_v0_x, mapped_v0_y};
                        Vec2F32 v1 = {mapped_v1_x, mapped_v1_y};
                        Vec2F32 v2 = {mapped_v2_x, mapped_v2_y};
                        
                        draw_triangle__scanline(buffer, v0, v1, v2, color);
                        //draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Barycentric);
                        
                        //Triangle_Mesh triangle = {v0, v1, v2, blue};
                        //prepare_scene_for_this_frame(&triangle, width, height);
                    }
                    //EndTime();
                }
            }
        }
        EndTime();
    }
    for (u32 i = 0; i < 20; i++)
    {
        if (anchors[i].result == 0) continue;
        printf("[%s] hit per frame: %d, time_total: %.5fms  time_average: %.5fms\n", anchors[i].name, anchors[i].count, timer_os_time_to_ms(anchors[i].result), timer_os_time_to_ms(anchors[i].result) / anchors[i].count);
        anchors[i].result = 0;
        anchors[i].count = 0;
    }
    //LONGLONG model_end = timer_get_os_time();
    //LONGLONG result = model_end - model_now;
    
    //printf("per frame time: %.2fms\n", timer_os_time_to_ms(result));
}