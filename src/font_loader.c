#include "font_loader.h"

internal void 
font_init(void)
{
    FT_Error error = FT_Init_FreeType(&library);
    if (error)
    {
        const char* err_str = FT_Error_String(error);
        printf("FT_Init_FreeType: %s\n", err_str);
        exit(1);
    }
}

internal FontInfo
font_load(Arena* arena, const char *font_path)
{
    FontInfo info = {0};
    // TODO temp memory instead?
    OS_FileReadResult font_file = os_file_read(arena, font_path);

    FT_Face face = {0};
    if(font_file.data)
    {
        FT_Open_Args args = {0};
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (u8*) font_file.data;
        args.memory_size = font_file.size;

        FT_Error opened_face = FT_Open_Face(library, &args, 0, &face);
        if (opened_face) 
        {
            const char* err_str = FT_Error_String(opened_face);
            printf("FT_Open_Face: %s\n", err_str);
            exit(1);
        }

        FT_Error set_char_size_err = FT_Set_Char_Size(face, 4 * 64, 4 * 64, 300, 300);
        info.ascent = face->size->metrics.ascender >> 6;    // Distance from baseline to top (positive)
        info.descent = - (face->size->metrics.descender >> 6); // Distance from baseline to bottom (positive)
        //u32 text_height = face->size->metrics.height >> 6;
        //u32 line_skip = text_height - (info.ascent + info.descent);
        //info.line_height = text_height + line_skip;
        info.line_height = face->size->metrics.height >> 6;

        {
            //aim_profiler_time_block("font_table creation")
            for(u32 codepoint = '!'; codepoint <= '~'; codepoint++) {
                //font_table[u32(codepoint)] = load_glyph(face, char(codepoint));
                info.font_table[(u32)codepoint] = font_load_glyph(face, (char)codepoint, &info);
            }
            info.font_table[(u32)(' ')] = font_load_glyph(face, (char)(' '), &info);
        }
    }
    return info;
}

FontGlyph font_load_glyph(FT_Face face, char codepoint, FontInfo *info) {
    FontGlyph result = {0};
    // The index has nothing to do with the codepoint
    u32 glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index) 
    {
        FT_Error load_glyph_err = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT); 
        if (load_glyph_err) 
        {
            const char* err_str = FT_Error_String(load_glyph_err);
            printf("FT_Load_Glyph: %s\n", err_str);
        }

        FT_Error render_glyph_err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (render_glyph_err) 
        {
            const char* err_str = FT_Error_String(render_glyph_err);
            printf("FT_Load_Glyph: %s\n", err_str);
        }

		info->max_glyph_width = Max(info->max_glyph_width, face->glyph->bitmap.width);
		info->max_glyph_height = Max(info->max_glyph_height, face->glyph->bitmap.rows);

        result.bitmap.width = face->glyph->bitmap.width;
        result.bitmap.height = face->glyph->bitmap.rows;
        result.bitmap.pitch = face->glyph->bitmap.pitch;
        
        result.bitmap_top = face->glyph->bitmap_top;
        result.bitmap_left = face->glyph->bitmap_left;
        result.advance_x = face->glyph->advance.x;
        info->max_char_width = Max(info->max_char_width, result.advance_x);

        FT_Bitmap* bitmap = &face->glyph->bitmap;
        result.bitmap.buffer = (u8*)malloc(result.bitmap.pitch * result.bitmap.height);
        memcpy(result.bitmap.buffer, bitmap->buffer, result.bitmap.pitch * result.bitmap.height);
    }
    return result;
}
