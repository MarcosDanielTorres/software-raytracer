#pragma once
typedef struct Vec4 Vec4;
struct Vec4
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

typedef struct Vec3 Vec3;
struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};

inline Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return (Vec3) {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return (Vec3) {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline f32 vec3_dot(Vec3 a, Vec3 b)
{
    return (f32) {a.x * b.x + a.y * b.y + a.z * b.z};
}

inline Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    Vec3 result = {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x
    };
    return result;
}

inline f32 vec3_magnitude_squared(Vec3 a)
{
    f32 result = vec3_dot(a, a);
    return result;
}

inline f32 vec3_magnitude(Vec3 a)
{
    f32 result = sqrtf(vec3_magnitude_squared(a));
    return result;
}

inline Vec3 vec3_normalize(Vec3 a)
{
    Vec3 result = { 0};
    f32 magnitude = vec3_magnitude_squared(a);
    if(magnitude > 0.00001)
    {
        f32 denominator = (1.0f / magnitude);
        result.x = a.x * denominator;
        result.y = a.y * denominator;
        result.z = a.z * denominator;
    }
    return result;
}

typedef struct Point3D Point3D;
struct Point3D
{
    f32 x;
    f32 y;
    f32 z;
};

typedef struct Mat4 Mat4;
struct Mat4
{
    f32 m[4][4];
};

Mat4 mat4_identity()
{
    Mat4 result = {0};
    result.m[0][0] = 1;
    result.m[1][1] = 1;
    result.m[2][2] = 1;
    result.m[3][3] = 1;
    return result;
}

Mat4 mat4_rotation_y(f32 angle)
{
    f32 c = cos(angle);
    f32 s = sin(angle);
    Mat4 result = {{
            {c, 0, s, 0},
            {0, 1, 0, 0},
            {-s, 0, c, 0},
            {0, 0, 0, 1},
        }};
    return result;
}

Mat4 mat4_make_perspective(f32 fov, f32 aspect, f32 znear, f32 zfar) {
    // | (h/w)*1/tan(fov/2)             0              0                 0 |
    // |                  0  1/tan(fov/2)              0                 0 |
    // |                  0             0     zf/(zf-zn)  (-zf*zn)/(zf-zn) |
    // |                  0             0              1                 0 |
    Mat4 m = {{{ 0 }}};
    m.m[0][0] = aspect * (1 / tan(fov / 2));
    m.m[1][1] = 1 / tan(fov / 2);
    m.m[2][2] = zfar / (zfar - znear);
    m.m[2][3] = (-zfar * znear) / (zfar - znear);
    m.m[3][2] = 1.0;
    return m;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up) {
    // Compute the forward (z), right (x), and up (y) vectors
    Vec3 z = vec3_sub(target, eye);
    z = vec3_normalize(z);
    Vec3 x = vec3_cross(up, z);
    x = vec3_normalize(x);
    Vec3 y = vec3_cross(z, x);
    
    // | x.x   x.y   x.z  -dot(x,eye) |
    // | y.x   y.y   y.z  -dot(y,eye) |
    // | z.x   z.y   z.z  -dot(z,eye) |
    // |   0     0     0            1 |
    Mat4 view_matrix = {{
            { x.x, x.y, x.z, -vec3_dot(x, eye) },
            { y.x, y.y, y.z, -vec3_dot(y, eye) },
            { z.x, z.y, z.z, -vec3_dot(z, eye) },
            {   0,   0,   0,                 1 }
        }};
    return view_matrix;
}

Vec4 mat4_mul_vec4(Mat4 m, Vec4 v) {
    Vec4 result;
    result.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w;
    result.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w;
    result.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w;
    result.w = m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w;
    return result;
}

Mat4 mat4_mul_mat4(Mat4 a, Mat4 b) {
    Mat4 m;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        }
    }
    return m;
}