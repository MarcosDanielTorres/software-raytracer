#pragma once
// TODO
// Size of allocations
// - Decide what to do with the 1-based indexing of things!
// - It seems very good to have an array structure instead of having to maintain a data and count for each
// fucking thing i want. I would be nice to have something like: texture_coordinates.data, texture_coordinates.size instead of
// texture_coordinates and texture_coordinates_count because I see myself forgetting the names of the variables and not being consistent
// as well

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
    Vec3 *vertices;
    u32 vertex_count;
    Vec2 *texture_coordinates;
    u32 texture_coordinates_count;
    Face *faces;
    u32 face_count;
    b32 is_valid;
    b32 has_normals;
};

inline b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
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
                        model.texture_coordinates[model.texture_coordinates_count] = (Vec2){uvs[0], uvs[1]};
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
				printf("Unrecognized symbol %c\n", *at);
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