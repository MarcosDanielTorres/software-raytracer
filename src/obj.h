#pragma once

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
    Face *faces;
    u32 face_count;
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
    model.vertices = (Vec3*)malloc(sizeof(Vec3) * 9000);
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
                        model.vertices[model.vertex_count++] = (Vec3){coords[0], coords[1], coords[2]};
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
				model.faces[model.face_count++] = face;
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