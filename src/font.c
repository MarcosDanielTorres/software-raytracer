#include "font.h"
/* NOTEs
* - face->glyph->bitmap
* --- face->glyph->bitmap.buffer
* - Global Metrics: face->(ascender, descender, height): For general font information
* - Scaled Global Metrics: face->size.metrics.(ascender, descender, height): For rendering text
* - "The metrics found in face->glyph->metrics are normally expressed in 26.6 pixel format"
    to convert to pixels >> 6 (divide by 64). Applies to scaled metrics as well.
* - Calling `FT_Set_Char_Size` sets `FT_Size` in the active `FT_Face` and is used by functions like: `FT_Load_Glyph`
    https://freetype.org/freetype2/docs/reference/ft2-sizing_and_scaling.html#ft_size
* - face->bitmap_top is the distance from the top of the bitmap to the baseline. y+ positive
    https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_glyphslot
* - glyph->advance.x must be shifted >> 6 as well if pixels are wanted
*/

internal TextSize 
font_get_text_size(FontInfo *font_info, String8 text)
{
    // todo remove from here put it in font_info
    u32 tab_size = 4;
    TextSize text_size = {0};
    u32 new_lines = 1;

    b32 multiline = 0;
    u32 max_width = 0;

    u32 char_count_per_line = 0;
    for(u32 i = 0; i < text.size; i++)
    {
        if(text.data[i] == '\n')
        {
            max_width = Max(max_width, char_count_per_line);
            char_count_per_line = 0;
            new_lines++;
            multiline = 1;
        }
        else
        {
            if(text.data[i] == '\t')
            {
                char_count_per_line += tab_size;
            }
            else
            {
                char_count_per_line++;
            }
        }
    }
    //text_size.w = f32((font_info->max_char_width >> 6) * (text_size));
    text_size.w = (f32)((font_info->max_char_width >> 6) * (multiline ? max_width : text.size));
    text_size.h = (f32)((font_info->descent + font_info->ascent) * new_lines);
    text_size.lines = new_lines;
    return text_size;
}

// TODO x y baseline los tengo que volar de aca por dios
internal TextBoundingBox
font_text_get_bounding_box(FontInfo *font_info, String8 text, f32 x, f32 baseline)
{
    TextBoundingBox result = {0};
    TextSize text_size = font_get_text_size(font_info, text);
    result = (TextBoundingBox) {.x = x, .y = baseline + font_info->descent, .w = text_size.w, .h = text_size.h};
    return result;
}
