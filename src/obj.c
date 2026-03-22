#include "obj.h"

internal inline u64 hash_obj_key(Obj_Key k)
{
    u32 h = 0;
    h ^= (u32)k.v  * 73856093u;
    h ^= (u32)k.vt * 19349663u;
    h ^= (u32)k.vn * 83492791u;
    h ^= h >> 16;
    return h;
}

internal inline i32 obj_key_equal(Obj_Key a, Obj_Key b)
{
    return a.v == b.v && a.vt == b.vt && a.vn == b.vn;
}

internal Mesh
obj_build_mesh(Arena *arena, Obj_Model model)
{
    Obj_VertexMap obj_map = {0};
    TempArena scratch = scratch_begin();

    Mesh mesh = {0};
    u32 vertices_size = (model.face_count + 1) * 3;
    mesh.vertices = arena_push_size(arena, Vertex, vertices_size);
    mesh.vertices_count = 0;
    mesh.indices = (u32*) arena_push_size(arena, u32, vertices_size);
    mesh.indices_count = 0;

    for(u32 f = 0; f <= model.face_count; f++)
    {
        Face face = model.faces[f];

        i32 face_0_p_index = face.v[0];
        i32 face_1_p_index = face.v[1];
        i32 face_2_p_index = face.v[2];
        i32 face_0_uv_index = face.vt[0];
        i32 face_1_uv_index = face.vt[1];
        i32 face_2_uv_index = face.vt[2];
        i32 face_0_n_index = face.vn[0];
        i32 face_1_n_index = face.vn[1];
        i32 face_2_n_index = face.vn[2];

        Obj_Key key_0 = {face_0_p_index, face_0_uv_index, face_0_n_index};
        Obj_Key key_1 = {face_1_p_index, face_1_uv_index, face_1_n_index};
        Obj_Key key_2 = {face_2_p_index, face_2_uv_index, face_2_n_index};
        
        Map_Result result = map_get_or_create(scratch.arena, &obj_map, key_0, mesh.vertices_count);
        if(result.created)
        {
            Vec3 p0 = model.vertices[face_0_p_index - 1];
            Vec2 uv0 = model.texture_coordinates[face_0_uv_index - 1];
            Vec3 n0 = model.normals[face_0_n_index - 1];
            Vertex new_vertex = (Vertex) { .position = p0, .uv = uv0, .normal = n0, .color = white };
            mesh.vertices[mesh.vertices_count++] = new_vertex;
        }
        mesh.indices[mesh.indices_count++] = result.index_into_vertex_array;

        result = map_get_or_create(scratch.arena, &obj_map, key_1, mesh.vertices_count);
        if(result.created)
        {
            Vec3 p1 = model.vertices[face_1_p_index - 1];
            Vec2 uv1 = model.texture_coordinates[face_1_uv_index - 1];
            Vec3 n1 = model.normals[face_1_n_index - 1];
            Vertex new_vertex = (Vertex) { .position = p1, .uv = uv1, .normal = n1, .color = white };
            mesh.vertices[mesh.vertices_count++] = new_vertex;
        }
        mesh.indices[mesh.indices_count++] = result.index_into_vertex_array;

        result = map_get_or_create(scratch.arena, &obj_map, key_2, mesh.vertices_count);
        if(result.created)
        {
            Vec3 p2 = model.vertices[face_2_p_index - 1];
            Vec2 uv2 = model.texture_coordinates[face_2_uv_index - 1];
            Vec3 n2 = model.normals[face_2_n_index - 1];
            Vertex new_vertex = (Vertex) { .position = p2, .uv = uv2, .normal = n2, .color = white };
            mesh.vertices[mesh.vertices_count++] = new_vertex;
        }
        mesh.indices[mesh.indices_count++] = result.index_into_vertex_array;
    }
    scratch_end(scratch);
    return mesh;
}

internal Map_Result
map_get_or_create(Arena *arena, Obj_VertexMap *map, Obj_Key key, u32 index)
{
    Map_Result result = {0};
    u64 bucket = hash_obj_key(key) & (ArrayCount(map->table) - 1);
    Obj_Vertex_Map_Entry **slot = &map->table[bucket];
    for(Obj_Vertex_Map_Entry *entry = *slot; entry; entry = entry->next)
    {
		if(obj_key_equal(entry->key, key))
		{
            result.created = 0;
            result.index_into_vertex_array = entry->index_into_vertex_array;
            return result;
		}
    }
    Obj_Vertex_Map_Entry *entry = arena_push_size(arena, Obj_Vertex_Map_Entry, 1);
    entry->key = key;
    entry->index_into_vertex_array = index;
    result.created = 1;
    result.index_into_vertex_array = index;
    entry->next = *slot;
    *slot = entry;
    return result;
}



internal void obj_parser_eat_until_newline(char** at2)
{
    u8* at = *at2;
    while (*at != '\n' && *at != '\0') { at++; };
    *at2 = at;
}

internal f32 obj_parser_convert_cstring_to_f32(char *start, u32 *movement)
{
    f32 result = {0};
#if 1
    u8* at = start;
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
            result *= 10.0f;
            result += (f32)(*at - '0');
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
        result = result / inc;
    }
    result *= sign;

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
            result *= divisor;
        }
        else
        {
            result /= divisor;
        }
    }
    *movement = at - start;
#else
    f32 num;
    char *pEnd;
    num = strtof(start, &pEnd);
    *movement = (u32)(pEnd - start);
    result = num;
#endif

    return result;
}

Obj_Model parse_obj(char *buf, size_t size)
{
    // TODO come on this should be toggled or something hahaha
    LONGLONG start = timer_get_os_time();
    //aim_profiler_time_function;
    Obj_Model model = {0};
    if(size == 0)
    {
        return model;
    }
    model.vertices = (Vec3*)malloc(sizeof(Vec3) * 9000);
    model.texture_coordinates = (Vec2*)malloc(sizeof(Vec2) * 9000);
    model.normals = (Vec3*)malloc(sizeof(Vec3) * 9000);
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
                        int i = 0;
                        while(i < 3)
                        {
                            at++;
                            u32 movement;
                            coords[i] = obj_parser_convert_cstring_to_f32(at, &movement);
                            at += movement;
                            i++;
                        }
                        at++;
                        model.vertices[model.vertex_count++] = (Vec3){coords[0], coords[1], coords[2]};
                    }break;
                    case 't':
                    {
                        at++;
                        f32 uvs[2];
                        int i = 0;
                        while(i < 2)
                        {
                            at++;
                            u32 movement;
                            uvs[i] = obj_parser_convert_cstring_to_f32(at, &movement);
                            at += movement;
                            i++;
                        }
                        model.texture_coordinates[model.texture_coordinates_count++] = (Vec2){uvs[0], uvs[1]};
                        at++;
                    } break;
                    case 'n':
                    {
                        at++;
                        f32 normals[3];
                        int i = 0;
                        while(i < 2)
                        {
                            at++;
                            u32 movement;
                            normals[i] = obj_parser_convert_cstring_to_f32(at, &movement);
                            at += movement;
                            i++;
                        }
                        model.normals[model.normal_count++] = (Vec3){normals[0], normals[1], normals[2]};
                        at++;
                    } break;
                    default:
                    {
                        at++;
                    }
			    } break;
            }
            case 'm':
            {
                obj_parser_eat_until_newline(&at);
            } break;
            case 'o':
            {
                obj_parser_eat_until_newline(&at);
            } break;
            case 'u':
            {
                obj_parser_eat_until_newline(&at);
            } break;
            case 's':
            {
                obj_parser_eat_until_newline(&at);
            } break;
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
				model.faces[model.face_count++] = face;
			} break;
			case '#':
			{
                obj_parser_eat_until_newline(&at);
			} break;
			default:
			{
			//	printf("Unrecognized symbol %c\n", *at);
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