#include "base_core.h"
#include "base_os.h"
#include "platform.h"

typedef struct Point3D Point3D;
struct Point3D
{
    float x;
    float y;
    float z;
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
    Point3D *points;
    int points_count;
    Face *faces;
    int faces_count;
    b32 is_valid;
};

typedef struct File_Contents File_Contents;
struct File_Contents
{
    char *data;
    size_t size;
};

File_Contents read_file(const char *filename)
{
    File_Contents result = {0};
    FILE *file = fopen(filename, "rb");
    if (!file) 
    {
        printf("Probably not found: %s\n", filename);
        return result;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buf = (char*)malloc(size + 1);
    fread(buf, 1, size, file);

    result.data = buf;
    result.size = size;
    return result;
}

b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
}

Obj_Model parse_obj(char *buf, size_t size)
{
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
                        float coords[3];
                        float coords2[3];
                        int i = 0;
                        while(i < 3)
                        {
                            at++;
                            char *start = at;

                            float a = 0;
                            b32 found = 0;
                            int past_point = 0;
                            int inc = 1;

                            float sign = 1;
                            if (*at == '-')
                            {
                                sign = -1;
                            }
                            while (*at != '\n' && *at != '\0' && *at != ' ')
                            {
                                if (is_digit(*at))
                                {
                                    a *= 10.0f;
                                    a += (float)(*at - '0');
                                    if (found)
                                        inc *= 10;

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
                            coords[i] = a;


                            float num;
                            char *pEnd;
                            num = strtof(start, &pEnd);
                            coords2[i] = num;
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
    
    model.is_valid = 1;
    return model;
};

float map_range(float val, float src_range_x, float src_range_y, float dst_range_x, float dst_range_y)
{
    float t = (val - src_range_x) / (src_range_y - src_range_x);
    float result = dst_range_x + t * (dst_range_y - dst_range_x);
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
	u32 x_min = (u32)roundf(x);
	u32 y_min = (u32)roundf(y);
    if (x_min < 0) x_min = 0;
    if (x_min > buffer->width - 1) x_min = buffer->width - 1;
    if (y_min < 0) y_min = 0;
    if (y_min > buffer->height - 1) y_min = buffer->height - 1;

	u32 *ptr = buffer->data + y_min * buffer->width + x_min;
    *ptr = color;
}

typedef struct Vec2F32 Vec2F32;
struct Vec2F32
{
    f32 x;
    f32 y;
};

f32 dot(Vec2F32 a, Vec2F32 b)
{
    return a.x * b.x + a.y * b.y;
}

f32 len_sq(Vec2F32 a)
{
    return dot(a, a);
}

f32 len(Vec2F32 a)
{
    return sqrtf(dot(a, a));
}

Vec2F32 norm(Vec2F32 a)
{
    f32 vlen = len(a);
    if (fabsf(vlen) < 0.0001)
    {
        return (Vec2F32){0};
    }
    Vec2F32 result = {a.x * 1.0f / vlen, a.y * 1.0f / vlen};
    return result;
}


Vec2F32 vec_sub(Vec2F32 a, Vec2F32 b)
{
    return (Vec2F32) {a.x - b.x, a.y - b.y};
}

Vec2F32 vec_add(Vec2F32 a, Vec2F32 b)
{
    return (Vec2F32) {a.x + b.x, a.y + b.y};
}

Vec2F32 vec_scalar(Vec2F32 b, float s)
{
    return (Vec2F32) {s * b.x, s * b.y};
}

void vec2f32_compare_and_swap_x(Vec2F32 *a, Vec2F32 *b)
{
    if (a->x > b->x)
    {
        Vec2F32 temp = *a;
        *a = *b;
        *b = temp;
    }
}

void vec2f32_compare_and_swap(Vec2F32 *a, Vec2F32 *b)
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
    //aim_profiler_time_function;


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

global Obj_Model model;

UPDATE_AND_RENDER(update_and_render)
{
    // remove this must come from the invoker
    local_persist b32 init = 0;
    if(!init)
    {
        const char *filename = ".\\obj\\african_head\\african_head.obj";
        //filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
        File_Contents obj = read_file(filename);
        model = parse_obj(obj.data, obj.size);
        init = 1;
    }
    u32 black = 0xff000000;
    u32 white = 0xffffffff;
    u32 red = 0xffff0000;
    u32 green = 0xff00ff00;
    u32 blue = 0xff0000ff;
    draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, 0xff111f0f);
    draw_pixel(buffer, buffer->width / 2, buffer->height / 2, blue);
    draw_rectangle(buffer, 120, 130, 50, 50, 0xffffff00);

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

            float mapped_v0_x;
            float mapped_v0_y;
            float mapped_v1_x;
            float mapped_v1_y;
            float mapped_v2_x;
            float mapped_v2_y;

            {
                mapped_v0_x = map_range(v0.x, -1.0f, 1.0f, 0.0f, buffer->width);
                //mapped_v0_y = map_range(v0.y, -1.0f, 1.0f, 0.0f, buffer->height);
                mapped_v0_y = map_range(v0.y, -1.0f, 1.0f, buffer->height, 0.0f);
                mapped_v1_x = map_range(v1.x, -1.0f, 1.0f, 0.0f, buffer->width);
                //mapped_v1_y = map_range(v1.y, -1.0f, 1.0f, 0.0f, buffer->height);
                mapped_v1_y = map_range(v1.y, -1.0f, 1.0f, buffer->height, 0.0f);
                mapped_v2_x = map_range(v2.x, -1.0f, 1.0f, 0.0f, buffer->width);
                //mapped_v2_y = map_range(v2.y, -1.0f, 1.0f, 0.0f, buffer->height);
                mapped_v2_y = map_range(v2.y, -1.0f, 1.0f, buffer->height, 0.0f);
            }
            //printf("First coord mapped %.4f -> %.4f\n", v0.x, mapped_v0_x);
            {
                Vec2F32 v0 = {mapped_v0_x, mapped_v0_y};
                Vec2F32 v1 = {mapped_v1_x, mapped_v1_y};
                Vec2F32 v2 = {mapped_v2_x, mapped_v2_y};

                draw_triangle__scanline(buffer, v0, v1, v2, blue);
                //draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Barycentric);

                //Triangle_Mesh triangle = {v0, v1, v2, blue};
                //prepare_scene_for_this_frame(&triangle, width, height);
            }
        }
    }
}