#pragma once
#undef internal
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static
#include "font.h"

global FT_Library library;

internal void font_init(void);
internal FontInfo font_load(Arena *arena, const char *font_path);
internal FontGlyph font_load_glyph(FT_Face face, char codepoint, FontInfo *info);
