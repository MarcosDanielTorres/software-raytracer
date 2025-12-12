#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <assert.h>

typedef uint32_t u32;
typedef int32_t i32;
typedef uint32_t b32;
typedef float f32;

#define kb(value) (value * 1024LL)
#define mb(value) (kb(value) * 1024LL)
#define gb(value) (mb(value) * 1024LL)

#define Min(a, b) (a < b ? (a) : (b))
#define Max(a, b) (a > b ? (a) : (b))
#define Sign(type, x) (((type)(x > 0)) - ((type)(x) < 0))
#define ClampBot(a, b) (Max(a, b))
#define ClampTop(a, b) (Min(a, b))
#define Clamp(a, b, c) (ClampTop(ClampBot(a, b), c))

#include "aim_timer.h"
#include "aim_timer.cpp"
#define AIM_PROFILER 1
#include "aim_profiler.h"
#include "aim_profiler.cpp"
#include "tgaimage.h"
#include "tgaimage.cpp"

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

static int barycentric_total_invocations = 0;

typedef u32 TriangleRasterizationAlgorithm;
enum
{
    TriangleRasterizationAlgorithm_Scanline,
    TriangleRasterizationAlgorithm_Barycentric,
};

struct Point3D
{
    float x;
    float y;
    float z;
};

struct Face
{
    // vvv,vtvtvt,vnvnvn
    int packed_data[9];
    int v[3];
    int vt[3];
    int vn[3];
};

struct Obj_Model
{
    Point3D *points;
    int points_count;
    Face *faces;
    int faces_count;
};

struct File_Contents
{
    char *data;
    size_t size;
};

File_Contents read_file(const char *filename)
{
    File_Contents result = {};
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

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

Obj_Model parse_obj(char *buf, size_t size)
{
    aim_profiler_time_function;
    Obj_Model model = {};
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
                            bool found = false;
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
                                    found = true;
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
                        model.points[model.points_count] = {coords[0], coords[1], coords[2]};
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
    
    return model;
};

float map_range(float val, float src_range_x, float src_range_y, float dst_range_x, float dst_range_y)
{
    float t = (val - src_range_x) / (src_range_y - src_range_x);
    float result = dst_range_x + t * (dst_range_y - dst_range_x);
    return result;
}

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
        return Vec2F32{};
    }
    Vec2F32 result = {a.x * 1.0f / vlen, a.y * 1.0f / vlen};
    return result;
}

Vec2F32 operator-(Vec2F32 a, Vec2F32 b)
{
    return Vec2F32 {a.x - b.x, a.y - b.y};
}

Vec2F32 operator+(Vec2F32 a, Vec2F32 b)
{
    return Vec2F32 {a.x + b.x, a.y + b.y};
}

Vec2F32 operator*(float s, Vec2F32 b)
{
    return Vec2F32 {s * b.x, s * b.y};
}
Vec2F32 operator*(Vec2F32 b, float s)
{
    return Vec2F32 {s * b.x, s * b.y};
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

void sort_3(f32 *a, f32 *b, f32 *c)
{
    if (*b < *a)
    {
        f32 temp_a = *a;
        *a = *b;
        *b = temp_a;
    }
    if (*c < *a)
    {
        f32 temp_a = *a;
        *a = *c;
        *c = temp_a;
    }
    if (*c < *b)
    {
        f32 temp_b = *b;
        *b = *c;
        *c = temp_b;
    }
}

void scanline_bottom_flat(TGAImage* framebuffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, TGAColor c0)
{
	// bottom flat
	Vec2F32 points_C_to_A[30000];
	u32 count_C_to_A = 0;
	Vec2F32 points_C_to_B[30000];
	u32 count_C_to_B = 0;
	{
		f32 x = C.x;
		f32 y = C.y;
		Vec2F32 aux = A - C;
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
		Vec2F32 aux = B - C;
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
		assert(count_C_to_A == count_C_to_B);

		for(u32 i = 0; i < min_count; i++)
		{
			Vec2F32 left = points_C_to_A[i];
			Vec2F32 right = points_C_to_B[i];
             assert(points_C_to_A[i].y == points_C_to_B[i].y);

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
                        framebuffer->set(x, left.y, c0);
				//framebuffer->line(left.x, left.y, right.x, right.y, c0);
			}
		}

	}

}

void scanline_top_flat(TGAImage* framebuffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, TGAColor c0)
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
			Vec2F32 aux = B - A;
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
			Vec2F32 aux = C - A;
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

		    assert(count_A_to_C == count_A_to_B);
			for(u32 i = 0; i < min_count; i++)
			{
				Vec2F32 left = points_A_to_B[i];
				Vec2F32 right = points_A_to_C[i];

             assert(left.y == right.y);
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
                        framebuffer->set(x, left.y, c0);
					//framebuffer->line(left.x, left.y, right.x, right.y, c0);
				}
			}

		}
	}

}

#if 1
void draw_triangle__scanline(TGAImage *framebuffer, Vec2F32 A, Vec2F32 C, Vec2F32 B, TGAColor c0)
{
    aim_profiler_time_function;


    vec2f32_compare_and_swap(&A, &B);
    vec2f32_compare_and_swap(&A, &C);
    vec2f32_compare_and_swap(&B, &C);

    if(A.y == B.y)
    {
        scanline_bottom_flat(framebuffer, A, B, C, c0);
    }
    else
    {
		if(B.y == C.y)
		{
			scanline_top_flat(framebuffer, A, B, C, c0);
		}
		else
		{
			Vec2F32 seg = C - A;
			f32 t = (B.y - A.y) / (C.y - A.y);
			Vec2F32 D = A + seg * t;
			scanline_top_flat(framebuffer, A, B, D, c0);
			scanline_bottom_flat(framebuffer, B, D, C, c0);
		}
    }
    //framebuffer->line(A.x, A.y, B.x, B.y, green);
    //framebuffer->line(C.x, C.y, B.x, B.y, green);
    //framebuffer->line(C.x, C.y, A.x, A.y, green);

    //framebuffer->set(A.x, A.y, green);
    //framebuffer->set(B.x, B.y, green);
    //framebuffer->set(C.x, C.y, green);
}

i32 cross(Vec2F32 A, Vec2F32 B)
{
    i32 result = (i32)(A.x * B.y - A.y * B.x);
    return result;
}

bool barycentric(Vec2F32 A, Vec2F32 B, Vec2F32 C, Vec2F32 P, i32 det, Vec2F32 AC, Vec2F32 AB)
{
    //Vec2F32 AC = C - A;
    //Vec2F32 AB = B - A;
    Vec2F32 AP = P - A;

    //i32 det = cross(AC, AB);
    i32 u = cross(AC, AP);
    i32 v = cross(AP, AB);
    i32 w = det - u - v;

    return ((Sign(i32, u) == Sign(i32, det) || u == 0) && (Sign(i32, v) == Sign(i32, det) || v == 0) && (Sign(i32, w) == Sign(i32, det) || w == 0));
}

double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    // AB x AC
    // (B.x - A.x, B.y - A.y) x (C.x - A.x, C.y - A.y) = (B.x - A.x)(C.y - A.y) - (B.y - A.y)(C.x - A.x)
    // (B.xC.y - B.xA.y - A.xC.y + A.xA.y) - (B.yC.x - B.yA.x -A.yC.x + A.yA.x)
    // B.xC.y - B.xA.y - A.xC.y + A.xA.y - B.yC.x + B.yA.x + A.yC.x - A.yA.x
    // (B.xC.y) - (B.xA.y) - (A.xC.y) + (A.xA.y) - (B.yC.x) + (B.yA.x) + (A.yC.x) - (A.yA.x)
    // 

    #if 1
    return 0.5 * ((bx-ax)*(cy-ay) - (by-ay)*(cx-ax));
    #else

    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
    #endif
}


void draw_triangle__barycentric_tile_render(TGAImage *framebuffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, TGAColor c0)
{
    aim_profiler_time_function;
    i32 minx = Min(Min(A.x, B.x), C.x);
    i32 miny = Min(Min(A.y, B.y), C.y);
    i32 maxx = Max(Max(A.x, B.x), C.x);
    i32 maxy = Max(Max(A.y, B.y), C.y);

    Vec2F32 AC = C - A;
    Vec2F32 AB = B - A;

    i32 det = cross(AC, AB);

    barycentric_total_invocations += (maxy - miny + 1) * (maxx - minx + 1);
    double total_area = signed_triangle_area(A.x, A.y, B.x, B.y, C.x, C.y);
    for(i32 pixel_row=miny; pixel_row<=maxy; pixel_row++) {
        for(i32 pixel_col=minx; pixel_col<=maxx; pixel_col++) {
            {
                #if 1
                    {
                        //aim_profiler_time_block("Barycentric");
                        if(barycentric(A, B, C, Vec2F32({(f32)pixel_col, (f32)pixel_row}), det, AC, AB))
                        {
                            framebuffer->set(pixel_col, pixel_row, c0);
                        }
                    }
                    #else
                    {
                        aim_profiler_time_block("Barycentric2");
                        double alpha = signed_triangle_area(pixel_col, pixel_row, B.x, B.y, C.x, C.y) / total_area;
                        double beta  = signed_triangle_area(pixel_col, pixel_row, C.x, C.y, A.x, A.y) / total_area;
                        double gamma = signed_triangle_area(pixel_col, pixel_row, A.x, A.y, B.x, B.y) / total_area;
                        if (alpha<0 || beta<0 || gamma<0) continue; 
                            framebuffer->set(pixel_col, pixel_row, c0);

                    }
                    #endif
            }
        }
    }
}

void draw_triangle__barycentric(TGAImage *framebuffer, Vec2F32 A, Vec2F32 B, Vec2F32 C, TGAColor c0)
{
    aim_profiler_time_function;
    i32 minx = Min(Min(A.x, B.x), C.x);
    i32 miny = Min(Min(A.y, B.y), C.y);
    i32 maxx = Max(Max(A.x, B.x), C.x);
    i32 maxy = Max(Max(A.y, B.y), C.y);

    Vec2F32 AC = C - A;
    Vec2F32 AB = B - A;

    i32 det = cross(AC, AB);

    barycentric_total_invocations += (maxy - miny + 1) * (maxx - minx + 1);
    double total_area = signed_triangle_area(A.x, A.y, B.x, B.y, C.x, C.y);
    for(i32 pixel_row=miny; pixel_row<=maxy; pixel_row++) {
        for(i32 pixel_col=minx; pixel_col<=maxx; pixel_col++) {
            {
                #if 1
                    {
                        //aim_profiler_time_block("Barycentric");
                        if(barycentric(A, B, C, Vec2F32({(f32)pixel_col, (f32)pixel_row}), det, AC, AB))
                        {
                            framebuffer->set(pixel_col, pixel_row, c0);
                        }
                    }
                    #else
                    {
                        aim_profiler_time_block("Barycentric2");
                        double alpha = signed_triangle_area(pixel_col, pixel_row, B.x, B.y, C.x, C.y) / total_area;
                        double beta  = signed_triangle_area(pixel_col, pixel_row, C.x, C.y, A.x, A.y) / total_area;
                        double gamma = signed_triangle_area(pixel_col, pixel_row, A.x, A.y, B.x, B.y) / total_area;
                        if (alpha<0 || beta<0 || gamma<0) continue; 
                            framebuffer->set(pixel_col, pixel_row, c0);

                    }
                    #endif
            }
        }
    }
}


struct Triangle_Mesh
{
    Vec2F32 v0;
    Vec2F32 v1;
    Vec2F32 v2;
    TGAColor color;

	Vec2F32 AC;
	Vec2F32 AB;

	i32 det;
	double total_area;
};

struct Rect2D
{
    u32 min_x;
    u32 min_y;
    u32 max_x;
    u32 max_y;
};

struct Render_Tile
{
    Triangle_Mesh *triangles;
    Rect2D *triangles_bbox;
    u32 min_x;
    u32 min_y;
    u32 max_x;
    u32 max_y;
    u32 count;
};

struct Render_Tile_Manager
{
    Render_Tile *tiles;
    u32 tile_dict[2000][2000];
    u32 count;
    u32 total_triangle_count;
    i32 dim_cell_x;
    i32 dim_cell_y;
    i32 partition_count;
};
static Render_Tile_Manager *tile_manager;

struct Render_Work_Queue_Item
{
    Render_Tile *tile;
};

struct Render_Work_Queue
{
    Render_Work_Queue_Item *items;
    volatile LONG count;
    u32 capacity;
    b32 is_work_to_do;
};

static Render_Work_Queue *render_work_queue;
struct Render_Thread_Context
{
    Render_Work_Queue *queue;
    TGAImage *framebuffer;
};


void init_scene(int width, int height, int partition_count)
{
    // this partition is square

    
    int dim_cell_x = width / partition_count;
    int dim_cell_y  = height / partition_count;
    tile_manager->dim_cell_x = dim_cell_x;
    tile_manager->dim_cell_y  = dim_cell_y;
    tile_manager->partition_count = partition_count;
    int squared_partition_count = partition_count * partition_count;
    tile_manager->tiles = (Render_Tile*)malloc(sizeof(Render_Tile) * squared_partition_count);
    for(u32 partition = 0; partition < squared_partition_count; partition++)
    {
        Render_Tile *tile = tile_manager->tiles + partition;
        tile->count = 0;
        // TODO: How do I define this?
        tile->triangles = (Triangle_Mesh*) malloc(sizeof(Triangle_Mesh) * 6000);
        tile->triangles_bbox = (Rect2D*) malloc(sizeof(Rect2D) * 6000);
        tile->min_x = dim_cell_x * (partition % partition_count);
        tile->min_y = dim_cell_y * (partition / partition_count);
        tile->max_x = tile->min_x + dim_cell_x;
        tile->max_y = tile->min_y + dim_cell_y;
        tile_manager->tile_dict[partition / partition_count][partition % partition_count] = tile_manager->count;
        tile_manager->count++;
    }
}

void prepare_scene_for_this_frame(Triangle_Mesh *triangle, int width, int height)
{
    aim_profiler_time_function;
    u32 minx = Clamp(Min(Min(triangle->v0.x, triangle->v1.x), triangle->v2.x), 0, width);
    u32 miny = Clamp(Min(Min(triangle->v0.y, triangle->v1.y), triangle->v2.y), 0, height);
    u32 maxx = Clamp(Max(Max(triangle->v0.x, triangle->v1.x), triangle->v2.x), 0, width);
    u32 maxy = Clamp(Max(Max(triangle->v0.y, triangle->v1.y), triangle->v2.y), 0, height);
    int pos_x = minx / (tile_manager->dim_cell_x);
    int pos_y = miny / (tile_manager->dim_cell_y);

    // TODO in order for this to work width and height must be integer divisible by partition_count
    // see if i can round the partition_count to the lower denominator. This will imply a new variable partition_dim
    assert(pos_x >= 0 && pos_x < tile_manager->partition_count && pos_y >= 0 && pos_y < tile_manager->partition_count);
    u32 index = tile_manager->tile_dict[pos_y][pos_x];
    Render_Tile *tile = &tile_manager->tiles[index];
    tile->triangles[tile->count] = *triangle;
    tile->triangles_bbox[tile->count] = {minx, miny, maxx, maxy};
    tile->count++;
    tile_manager->total_triangle_count++;
}


void draw_triangle(TGAImage *framebuffer, Vec2F32 A, Vec2F32 C, Vec2F32 B, TGAColor c0, TriangleRasterizationAlgorithm triangle_rasterization_algorithm)
{
    if(triangle_rasterization_algorithm == TriangleRasterizationAlgorithm_Scanline)
    {
        draw_triangle__scanline(framebuffer, A, C,  B, c0);
    }
    else if(TriangleRasterizationAlgorithm_Barycentric)
    {
        draw_triangle__barycentric(framebuffer, A, C,  B, c0);
    }
}

void render_frame_smart_tile_renderer(TGAImage *framebuffer)
{
    aim_profiler_time_function;
    for(u32 i = 0; i < tile_manager->count; i++)
    {
        Render_Tile *tile = &tile_manager->tiles[i];

        u32 min_x = tile->min_x;
        u32 min_y = tile->min_y;
        u32 max_x = tile->max_x;
        u32 max_y = tile->max_y;
		for(u32 j = 0; j < tile->count; j++)
		{
            Triangle_Mesh triangle = tile->triangles[j];
            Rect2D bbox = tile->triangles_bbox[j];


            // ------------------------------------------------------------
            // 1. Load triangle vertices as floats
            // ------------------------------------------------------------
            float x0 = triangle.v0.x;  float y0 = triangle.v0.y;
            float x1 = triangle.v1.x;  float y1 = triangle.v1.y;
            float x2 = triangle.v2.x;  float y2 = triangle.v2.y;

            // ------------------------------------------------------------
            // 2. Compute edge coefficients A,B,C for the 3 edges
            //    (floats for now; later convert to ints)
            // ------------------------------------------------------------
            float A0 = y0 - y1;    float B0 = x1 - x0;    float C0 = x0*y1 - x1*y0;
            float A1 = y1 - y2;    float B1 = x2 - x1;    float C1 = x1*y2 - x2*y1;
            float A2 = y2 - y0;    float B2 = x0 - x2;    float C2 = x2*y0 - x0*y2;

            // ------------------------------------------------------------
            // 3. Pixel center convention: add 0.5
            // ------------------------------------------------------------
            float px0 = bbox.min_x + 0.5f;
            float py0 = bbox.min_y + 0.5f;

            // ------------------------------------------------------------
            // 4. Evaluate edge functions at starting pixel (top-left of bbox)
            // ------------------------------------------------------------
            float E0_row = A0*px0 + B0*py0 + C0;
            float E1_row = A1*px0 + B1*py0 + C1;
            float E2_row = A2*px0 + B2*py0 + C2;

            // ------------------------------------------------------------
            // 5. Compute incremental steps
            //    Move right by +1 pixel
            // ------------------------------------------------------------
            float E0_dx = A0;
            float E1_dx = A1;
            float E2_dx = A2;

            // ------------------------------------------------------------
            //    Move down by +1 pixel
            // ------------------------------------------------------------
            float E0_dy = B0;
            float E1_dy = B1;
            float E2_dy = B2;



            Vec2F32 AC = triangle.AC;
            Vec2F32 AB = triangle.AB;

            i32 det = triangle.det;
            double total_area = triangle.total_area;
            //for (i32 pixel_row = min_y; pixel_row <= max_y; pixel_row++)
            for (i32 pixel_row = bbox.min_y; pixel_row <= bbox.max_y; pixel_row++)
            {
                // Edge values at the start of this scanline
                float E0 = E0_row;
                float E1 = E1_row;
                float E2 = E2_row;

                for (i32 pixel_col = bbox.min_x; pixel_col <= bbox.max_x; pixel_col++)
                {
                    if (E0 >= 0 && E1 >= 0 && E2 >= 0)
                    {
                        framebuffer->set(pixel_col, pixel_row, triangle.color);
                    }
                    E0 += E0_dx;
                    E1 += E1_dx;
                    E2 += E2_dx;

                    Vec2F32 pixel = { (f32)pixel_col, (f32)pixel_row };

                    barycentric_total_invocations++;
                    //aim_profiler_time_block("render_frame 2 [HOTLOOP]");
                    #if 0
                    {
                        //aim_profiler_time_function;


                        {
                            if (barycentric(triangle.v0, triangle.v1, triangle.v2, pixel, det, AC, AB))
                            {
                                framebuffer->set(pixel_col, pixel_row, triangle.color);
                            }
                        }
                    }
                    #endif
                }
                E0_row += E0_dy;
                E1_row += E1_dy;
                E2_row += E2_dy;
            }
        outer_loop:
        ;
        }
    }
}


void render_frame_smart_tile_renderer_multithreaded(TGAImage *framebuffer, Render_Tile *tile)
{
    //aim_profiler_time_function;
    {
        u32 min_x = tile->min_x;
        u32 min_y = tile->min_y;
        u32 max_x = tile->max_x;
        u32 max_y = tile->max_y;
		for(u32 j = 0; j < tile->count; j++)
		{
            Triangle_Mesh triangle = tile->triangles[j];
            Rect2D bbox = tile->triangles_bbox[j];


            // ------------------------------------------------------------
            // 1. Load triangle vertices as floats
            // ------------------------------------------------------------
            float x0 = triangle.v0.x;  float y0 = triangle.v0.y;
            float x1 = triangle.v1.x;  float y1 = triangle.v1.y;
            float x2 = triangle.v2.x;  float y2 = triangle.v2.y;

            // ------------------------------------------------------------
            // 2. Compute edge coefficients A,B,C for the 3 edges
            //    (floats for now; later convert to ints)
            // ------------------------------------------------------------
            float A0 = y0 - y1;    float B0 = x1 - x0;    float C0 = x0*y1 - x1*y0;
            float A1 = y1 - y2;    float B1 = x2 - x1;    float C1 = x1*y2 - x2*y1;
            float A2 = y2 - y0;    float B2 = x0 - x2;    float C2 = x2*y0 - x0*y2;

            // ------------------------------------------------------------
            // 3. Pixel center convention: add 0.5
            // ------------------------------------------------------------
            float px0 = bbox.min_x + 0.5f;
            float py0 = bbox.min_y + 0.5f;

            // ------------------------------------------------------------
            // 4. Evaluate edge functions at starting pixel (top-left of bbox)
            // ------------------------------------------------------------
            float E0_row = A0*px0 + B0*py0 + C0;
            float E1_row = A1*px0 + B1*py0 + C1;
            float E2_row = A2*px0 + B2*py0 + C2;

            // ------------------------------------------------------------
            // 5. Compute incremental steps
            //    Move right by +1 pixel
            // ------------------------------------------------------------
            float E0_dx = A0;
            float E1_dx = A1;
            float E2_dx = A2;

            // ------------------------------------------------------------
            //    Move down by +1 pixel
            // ------------------------------------------------------------
            float E0_dy = B0;
            float E1_dy = B1;
            float E2_dy = B2;



            Vec2F32 AC = triangle.AC;
            Vec2F32 AB = triangle.AB;

            i32 det = triangle.det;
            double total_area = triangle.total_area;
            //for (i32 pixel_row = min_y; pixel_row <= max_y; pixel_row++)
            for (i32 pixel_row = bbox.min_y; pixel_row <= bbox.max_y; pixel_row++)
            {
                // Edge values at the start of this scanline
                float E0 = E0_row;
                float E1 = E1_row;
                float E2 = E2_row;

                for (i32 pixel_col = bbox.min_x; pixel_col <= bbox.max_x; pixel_col++)
                {
                    if (E0 >= 0 && E1 >= 0 && E2 >= 0)
                    {
                        framebuffer->set(pixel_col, pixel_row, triangle.color);
                    }
                    E0 += E0_dx;
                    E1 += E1_dx;
                    E2 += E2_dx;

                    Vec2F32 pixel = { (f32)pixel_col, (f32)pixel_row };

                    barycentric_total_invocations++;
                    //aim_profiler_time_block("render_frame 2 [HOTLOOP]");
                    #if 0
                    {
                        //aim_profiler_time_function;


                        {
                            if (barycentric(triangle.v0, triangle.v1, triangle.v2, pixel, det, AC, AB))
                            {
                                framebuffer->set(pixel_col, pixel_row, triangle.color);
                            }
                        }
                    }
                    #endif
                }
                E0_row += E0_dy;
                E1_row += E1_dy;
                E2_row += E2_dy;
            }
        outer_loop:
        ;
        }
    }
}

void render_frame_smart_tile_renderer_without(TGAImage *framebuffer)
{
    aim_profiler_time_function;
    for(u32 i = 0; i < tile_manager->count; i++)
    //for(u32 i = 4; i < 5; i++)
    {
        Render_Tile *tile = &tile_manager->tiles[i];

        u32 min_x = tile->min_x;
        u32 min_y = tile->min_y;
        u32 max_x = tile->max_x;
        u32 max_y = tile->max_y;
		for(u32 j = 0; j < tile->count; j++)
		{
            Triangle_Mesh triangle = tile->triangles[j];
            Rect2D bbox = tile->triangles_bbox[j];
            //for (i32 pixel_row = min_y; pixel_row <= max_y; pixel_row++)
            for (i32 pixel_row = bbox.min_y; pixel_row <= bbox.max_y; pixel_row++)
            {
                for (i32 pixel_col = bbox.min_x; pixel_col <= bbox.max_x; pixel_col++)
                {
                    Vec2F32 pixel = { (f32)pixel_col, (f32)pixel_row };

                    barycentric_total_invocations++;
                    //aim_profiler_time_block("render_frame 2 [HOTLOOP]");
                    {
                        //aim_profiler_time_function;

                        Vec2F32 AC = triangle.AC;
                        Vec2F32 AB = triangle.AB;

                        i32 det = triangle.det;
                        double total_area = triangle.total_area;

                        {
                            if (barycentric(triangle.v0, triangle.v1, triangle.v2, pixel, det, AC, AB))
                            {
                                framebuffer->set(pixel_col, pixel_row, triangle.color);
                                break;
                            }
                        }
                    }
                }
            }
        }
        outer_loop:
        ;
    }
}

void render_frame_stupid_tile_renderer(TGAImage *framebuffer)
{
    aim_profiler_time_function;
    for(u32 i = 0; i < tile_manager->count; i++)
    {
        Render_Tile tile = tile_manager->tiles[i];

        u32 min_x = tile.min_x;
        u32 min_y = tile.min_y;
        u32 max_x = tile.max_x;
        u32 max_y = tile.max_y;

        for(i32 pixel_row=min_y; pixel_row<=max_y; pixel_row++) 
        {
            for(i32 pixel_col=min_x; pixel_col<=max_x; pixel_col++) 
            {
                Vec2F32 pixel = { (f32)pixel_col, (f32)pixel_row };
                for(u32 j = 0; j < tile.count; j++)
                {
                    barycentric_total_invocations ++;
                    //aim_profiler_time_block("render_frame [HOTLOOP]");
                    Triangle_Mesh triangle = tile.triangles[j];
                    {
                        //aim_profiler_time_function;

                        Vec2F32 AC = triangle.AC;
                        Vec2F32 AB = triangle.AB;

                        i32 det = triangle.det;
                        double total_area = triangle.total_area;

                        {
                            if(barycentric(triangle.v0, triangle.v1, triangle.v2, pixel, det, AC, AB))
                            {
                                framebuffer->set(pixel_col, pixel_row, triangle.color);
                                break;
                            }
                        }
                    }
                }
            }
        }
        outer_loop:
        ;
    }
}


#endif

DWORD WINAPI thread_proc(void *data)
{
    Render_Thread_Context *context = (Render_Thread_Context*) data;

    while(true)
    {
        {
			LONG index = InterlockedDecrement(&context->queue->count);
            if (index < 0) break;
			{
                DWORD threadId = GetCurrentThreadId();
                //printf("%d\n", threadId);
				render_frame_smart_tile_renderer_multithreaded(context->framebuffer, context->queue->items[index].tile);
			}
        }
    }
    return 0;
}

void add_work()
{
    for(u32 i = 0; i < tile_manager->count; i++)
    {
        Render_Tile *tile = &tile_manager->tiles[i];
        Render_Work_Queue_Item item = {tile};
        render_work_queue->items[i] = item;
    }
    InterlockedExchange(&render_work_queue->count, (LONG)tile_manager->count);
    render_work_queue->is_work_to_do = 1;
}

int main(int argc, char** argv) {


    aim_profiler_begin();
    LONGLONG freq = aim_timer_get_os_freq();
    const char *filename = ".\\obj\\african_head\\african_head.obj";
    //filename = ".\\obj\\diablo3_pose\\diablo3_pose.obj";
    File_Contents obj = read_file(filename);
    Obj_Model model = parse_obj(obj.data, obj.size);
    printf("model points: %d\n", model.points_count);
    printf("model triangles: %d\n", model.points_count / 3);
    printf("model faces: %d\n", model.faces_count);

    int partition_count = 60;
    int thread_count = 9;

    //constexpr int width  = 800;
    //constexpr int height = 600;
    constexpr int width  = 800;
    constexpr int height = 600;
    TGAImage framebuffer(width, height, TGAImage::RGB);

    tile_manager = (Render_Tile_Manager*)malloc(sizeof(Render_Tile_Manager));
    init_scene(width, height, partition_count);


    int points[6] =
    {
        0, 0, 12, 37, 62, 53
    };

    //framebuffer.line(points[0], points[1], points[2], points[3], green);
    //framebuffer.line(points[2], points[3], points[4], points[5], red);
    //framebuffer.line(points[4], points[5], points[0], points[1], blue);


    //framebuffer.set(points[0], points[2], white);
    //framebuffer.set(points[2], points[3], white);
    //framebuffer.set(points[4], points[5], white);

    TGAColor colors[5] = 
    {
        white,
        green,
        red,
        blue,
        yellow,
    };

    int col_in = 0;
    DWORD thread_id = 0;
    HANDLE *threads = (HANDLE*)malloc(sizeof(HANDLE) * thread_count);
    #if 1
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
                mapped_v0_x = map_range(v0.x, -1.0f, 1.0f, 0.0f, width);
                mapped_v0_y = map_range(v0.y, -1.0f, 1.0f, 0.0f, height);
                mapped_v1_x = map_range(v1.x, -1.0f, 1.0f, 0.0f, width);
                mapped_v1_y = map_range(v1.y, -1.0f, 1.0f, 0.0f, height);
                mapped_v2_x = map_range(v2.x, -1.0f, 1.0f, 0.0f, width);
                mapped_v2_y = map_range(v2.y, -1.0f, 1.0f, 0.0f, height);
            }
            //printf("First coord mapped %.4f -> %.4f\n", v0.x, mapped_v0_x);
            {
                Vec2F32 v0 = {mapped_v0_x, mapped_v0_y};
                Vec2F32 v1 = {mapped_v1_x, mapped_v1_y};
                Vec2F32 v2 = {mapped_v2_x, mapped_v2_y};

                TGAColor curr_color = colors[(col_in++) % 5];
                //draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Scanline);
                //draw_triangle(&framebuffer, v0, v1, v2, curr_color, TriangleRasterizationAlgorithm_Barycentric);
                Triangle_Mesh triangle = {v0, v1, v2, blue};

				triangle.AC = triangle.v2 - triangle.v0;
				triangle.AB = triangle.v1 - triangle.v0;

				triangle.det = cross(triangle.AC, triangle.AB);
				triangle.total_area = signed_triangle_area(triangle.v0.x, triangle.v0.y, triangle.v1.x, triangle.v1.y, triangle.v2.x, triangle.v2.y);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
                prepare_scene_for_this_frame(&triangle, width, height);
            }
        }

        render_work_queue = (Render_Work_Queue*)malloc(sizeof(Render_Work_Queue));
        render_work_queue->count = 0;
        render_work_queue->is_work_to_do = 0;
        render_work_queue->capacity = partition_count * partition_count + 1;
        render_work_queue->items = (Render_Work_Queue_Item*)malloc(sizeof(Render_Work_Queue_Item) * render_work_queue->capacity);

        Render_Thread_Context *context = (Render_Thread_Context*)malloc(sizeof(Render_Thread_Context));
        context->queue = render_work_queue;
        context->framebuffer = &framebuffer;

        add_work();
        for(u32 i = 0; i < thread_count; i++)
        {
            threads[i] = CreateThread(0, 0, thread_proc, context, 0, &thread_id);
        }

    }
    LONGLONG time_start = aim_timer_get_os_time();

    printf("Computed pixels: %d\n", barycentric_total_invocations);
    barycentric_total_invocations = 0;
    //render_frame_stupid_tile_renderer(&framebuffer);
    //render_frame_smart_tile_renderer(&framebuffer);
    barycentric_total_invocations = 0;
    printf("Computed pixels: %d\n", barycentric_total_invocations);
    printf("Computed pixels: %d\n", barycentric_total_invocations);
    printf("tile count: %d\n", tile_manager->count);
    #endif




    WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
    LONGLONG time_end = aim_timer_get_os_time();
    LONGLONG total_time = time_end - time_start;

    for(u32 i = 0; i < thread_count; i++) CloseHandle(threads[i]);
    for(u32 i = 0; i < tile_manager->count; i++)
    {
        Render_Tile tile = tile_manager->tiles[i];
        i32 min_x = tile.min_x;
        i32 min_y = tile.min_y;
        i32 max_x = tile.max_x;
        i32 max_y = tile.max_y;
        framebuffer.line(min_x, min_y, max_x, min_y, green);
        framebuffer.line(max_x, min_y, max_x, max_y, green);
        framebuffer.line(min_x, min_y, min_x, max_y, green);
        framebuffer.line(min_x, max_y, max_x, max_y, green);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    //framebuffer.write_tga_file_mmap("frame.tga", true, false);


    printf("total %.2f ms\n", aim_timer_ticks_to_ms(total_time, freq));
    aim_profiler_end();
    aim_profiler_print();
    return 0;
}

