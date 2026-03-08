#pragma once
typedef struct Bitmap Bitmap;
struct Bitmap {
    i32 width;
    i32 height;
    i32 pitch;
    u8* buffer;
};

typedef struct FontGlyph FontGlyph;
struct FontGlyph {
    Bitmap bitmap;
    i32 bitmap_top;
    i32 bitmap_left;
    i32 advance_x;

    f32 uv0_x;
    f32 uv1_x;
    f32 uv2_x;
    f32 uv3_x;

    f32 uv0_y;
    f32 uv1_y;
    f32 uv2_y;
    f32 uv3_y;
};

typedef struct FontInfo FontInfo;
struct FontInfo
{
    i32 ascent;
    i32 descent;
    i32 line_height;
    i32 max_glyph_width;
    i32 max_glyph_height;
    // probably renamed to max_char_advancement (same in monospace)
    i32 max_char_width;
    FontGlyph font_table[300];
};

// TODO maybe this could be transformed into a Dim2D
typedef struct TextSize TextSize;
struct TextSize
{
    f32 w;
    f32 h;
    u32 lines;
};

typedef struct TextBoundingBox TextBoundingBox;
struct TextBoundingBox
{
    f32 x;
    f32 y;
    f32 w;
    f32 h;
};

internal TextSize font_get_text_size(FontInfo *text_info, String8 text);
internal TextBoundingBox font_text_get_bounding_box(FontInfo *font_info, String8 text, f32 x, f32 baseline);