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
    Point3D *points;
    int points_count;
    Face *faces;
    int faces_count;
    b32 is_valid;
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
    model.points = (Point3D*)malloc(sizeof(Point3D) * 9000);
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
#if 1
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
                        model.points[model.points_count] = (Point3D){coords[0], coords[1], coords[2]};
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
					while (i < 3)
					{
						int index = 0;
                        while (*at != ' ' && *at != '/' && *at != '\n')
						{
							if (is_digit(*at))
							{
								index *= 10;
								index += (int)(*at - '0');
							}
                            at++;
						}
                        
                        if (i == 0)
                        {
                            *v++ = index;
                        }
                        else if (i == 1)
                        {
                            *vt++ = index;
                        }
                        else if (i == 2)
                        {
                            *vn++ = index;
                        }
                        
						face.packed_data[j * 3 + i] = index;
                        at++;
						i++;
					}
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
                int size = 1;
                while(*at != '\n' && *at != '\0')
                {
                    size++;
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
	u32 x_min = x;
	u32 y_min = y;
#endif
#if 1
    if (x_min < 0) x_min = 0;
    if (x_min > buffer->width - 1) x_min = buffer->width - 1;
    if (y_min < 0) y_min = 0;
    if (y_min > buffer->height - 1) y_min = buffer->height - 1;
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

//Point3D point_rotate_y(Point3D p, f32 angle)
Point3D point_scalar(Point3D p, f32 s)
{
    return (Point3D) {p.x * s, p.y * s, p.z * s};
}
Point3D point_rotate_y(Point3D p, f32 c, f32 s)
{
    Point3D result = {0};
    
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

global Obj_Model model;

struct App_Memory
{
    void *memory;
};

struct App_Input
{
    f32 total_time;
    f32 dt;
};

Point3D project_perspective_gl(Point3D p,
                               f32 fov_y_rad,
                               f32 aspect,
                               f32 z_near,
                               f32 z_far)
{
    f32 f = 1.0f / tanf(fov_y_rad * 0.5f);
    
    Point3D out;
    
    out.x = (p.x * f) / (aspect * -p.z);
    out.y = (p.y * f) / (-p.z);
    
    // OpenGL depth mapping to [-1,1]
    out.z = (z_far + z_near) / (z_near - z_far)
        + (2.0f * z_far * z_near) / (z_near - z_far) / p.z;
    
    return out; // this is NDC
}

typedef struct Entity Entity;
struct Entity
{
    const char* name;
    Obj_Model model;
    Point3D position;
};

global Entity entities[100];
global u32 entity_count;

UPDATE_AND_RENDER(update_and_render)
{
    // remove this must come from the invoker
    local_persist b32 init = 0;
    if(!init)
    {
        timer_init();
        const char *filename = ".\\obj\\african_head\\african_head.obj";
        //filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
        //filename = ".\\obj\\f117.obj";
        OS_FileReadResult obj = os_file_read(filename);
        model = parse_obj(obj.data, obj.size);
        
        {
            entities[entity_count++] = (Entity) { .name = "enemy_1", .model = model, .position = (Point3D) {0.0, 0.3, 5} };
            entities[entity_count++] = (Entity) { .name = "enemy_2", .model = model, .position = (Point3D) {-0.8, 0.3, 2} };
            entities[entity_count++] = (Entity) { .name = "enemy_3", .model = model, .position = (Point3D) {0.5, 0, 1} };
            entities[entity_count++] = (Entity) { .name = "enemy_4", .model = model, .position = (Point3D) {-0.5, 0.5, 3} };
        }
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
    local_persist int k = 1;
    if(accum_dt > 5.0f)
    {
        k = -1 * k;
        accum_dt = 0;
    }
    angle += k * dt;
    // questions:
    // what stage does the mapping from ndc to screen cordinates aka pixel buffer
    //  probably after perspective divide, altough i should really really really watch the pikuma course first because im skipping too many fundamentals sadly!
    // but first i will fix obj
    
    Vec3 camera_eye = (Vec3) {0, 0.0, 0};
    Vec3 camera_target = (Vec3) {0, 0, 1};
    camera_target = vec3_sub(camera_target, camera_eye);
    f32 fov = 3.141592 / 3.0; // 60 deg
    f32 aspect = (f32)buffer->height / (f32)buffer->width;
    f32 znear = 1.0f;
    f32 zfar = 50.0f;
    Mat4 persp = mat4_make_perspective(fov, aspect, znear, zfar);
    Mat4 view = mat4_look_at(camera_eye, camera_target, (Vec3) {0, 1, 0});
    
    LONGLONG model_now = timer_get_os_time();
	f32 c = cos(angle);
	f32 s = sin(angle);
	for (u32 entity = 0; entity < entity_count; entity++)
	{
        Entity* e = entities + entity;
		if(model.is_valid)
		{
			for(int face_index = 1; face_index <= model.faces_count; face_index++)
			{
				// TODO at face_index 375 minx ends up being -1004 which i guess thats why
				// i see a wrong obj
				Face face = model.faces[face_index];
				Point3D v0 = model.points[face.v[0]];
				Point3D v1 = model.points[face.v[1]];
				Point3D v2 = model.points[face.v[2]];
                
				v0 = point_rotate_y(v0, c, s);
				v1 = point_rotate_y(v1, c, s);
				v2 = point_rotate_y(v2, c, s);
                
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
                
                Vec4 transformed_v0 = mat4_mul_vec4(view, (Vec4){.x = v0.x, .y = v0.y, .z = v0.z, .w = 1});
                Vec4 transformed_v1 = mat4_mul_vec4(view, (Vec4){.x = v1.x, .y = v1.y, .z = v1.z, .w = 1});
                Vec4 transformed_v2 = mat4_mul_vec4(view, (Vec4){.x = v2.x, .y = v2.y, .z = v2.z, .w = 1});
                
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
                
                //assert(transformed_v0.x >= -1.0f && transformed_v0.x <= 1);
                //assert(transformed_v1.x >= -1.0f && transformed_v1.x <= 1);
                //assert(transformed_v2.x >= -1.0f && transformed_v2.x <= 1);
                
                
                //f32 z = 5;
				//v0.z -= z;
				//v1.z -= z;
				//v2.z -= z;
				//v0 = project_perspective_gl(v0, 0.78, 4.0 / 3.0, 0.001f, 100);
				//v1 = project_perspective_gl(v1, 0.78, 4.0 / 3.0, 0.001f, 100);
				//v2 = project_perspective_gl(v2, 0.78, 4.0 / 3.0, 0.001f, 100);
                
				f32 mapped_v0_x;
				f32 mapped_v0_y;
				f32 mapped_v1_x;
				f32 mapped_v1_y;
				f32 mapped_v2_x;
				f32 mapped_v2_y;
                
				{
					mapped_v0_x = map_range(transformed_v0.x, -1.0f, 1.0f, 0.0f, buffer->width);
					//mapped_v0_y = map_range(transformed_v0.y, -1.0f, 1.0f, 0.0f, buffer->height);
					mapped_v0_y = map_range(transformed_v0.y, -1.0f, 1.0f, buffer->height, 0.0f);
					mapped_v1_x = map_range(transformed_v1.x, -1.0f, 1.0f, 0.0f, buffer->width);
					//mapped_v1_y = map_range(transformed_v1.y, -1.0f, 1.0f, 0.0f, buffer->height);
					mapped_v1_y = map_range(transformed_v1.y, -1.0f, 1.0f, buffer->height, 0.0f);
					mapped_v2_x = map_range(transformed_v2.x, -1.0f, 1.0f, 0.0f, buffer->width);
					//mapped_v2_y = map_range(transformed_v2.y, -1.0f, 1.0f, 0.0f, buffer->height);
					mapped_v2_y = map_range(transformed_v2.y, -1.0f, 1.0f, buffer->height, 0.0f);
                    
					//mapped_v0_x = transformed_v0.x * (f32)buffer->width / 2.0f;
					//mapped_v0_y = transformed_v0.y * (f32)buffer->height / 2.0f;
					//mapped_v1_x = transformed_v1.x * (f32)buffer->width / 2.0f;
					//mapped_v1_y = transformed_v1.y * (f32)buffer->height / 2.0f;
					//mapped_v2_x = transformed_v2.x * (f32)buffer->width / 2.0f;
					//mapped_v2_y = transformed_v2.y * (f32)buffer->height / 2.0f;
                    
					//mapped_v0_x += (f32)buffer->width / 2.0f;
					//mapped_v0_y += (f32)buffer->height / 2.0f;
					//mapped_v1_x += (f32)buffer->width / 2.0f;
					//mapped_v1_y += (f32)buffer->height / 2.0f;
					//mapped_v2_x += (f32)buffer->width / 2.0f;
					//mapped_v2_y += (f32)buffer->height / 2.0f;
                    
                    
				}
				//printf("First coord mapped %.4f -> %.4f\n", v0.x, mapped_v0_x);
				{
					Vec2F32 v0 = {mapped_v0_x, mapped_v0_y};
					Vec2F32 v1 = {mapped_v1_x, mapped_v1_y};
					Vec2F32 v2 = {mapped_v2_x, mapped_v2_y};
                    
					draw_triangle__scanline(buffer, v0, v1, v2, red);
					//draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Barycentric);
                    
					//Triangle_Mesh triangle = {v0, v1, v2, blue};
					//prepare_scene_for_this_frame(&triangle, width, height);
				}
			}
        }
    }
    LONGLONG model_end = timer_get_os_time();
    LONGLONG result = model_end - model_now;
    
    //printf("rendering took: %.2fms\n", timer_os_time_to_ms(result));
}