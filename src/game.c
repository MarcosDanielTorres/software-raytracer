#include "base_core.h"
#include "base_os.h"
#include "platform.h"

static void draw_rectangle(Software_Render_Buffer *buffer, f32 x, f32 y, f32 width, f32 height, u32 color)
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

UPDATE_AND_RENDER(update_and_render)
{
    draw_rectangle(buffer, 0, 0, buffer->width, buffer->height, 0xff111f0f);
    draw_rectangle(buffer, 120, 130, 50, 50, 0xffffff00);
}