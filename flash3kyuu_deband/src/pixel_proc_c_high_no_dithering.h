

namespace pixel_proc_high_no_dithering {
	
	static inline void init_context(char context_buffer[CONTEXT_BUFFER_SIZE], int frame_width, int output_depth)
	{
		// nothing to do
	}

	static inline void destroy_context(void* context)
	{
		// nothing to do
	}

	static inline void next_pixel(void* context)
	{
		// nothing to do
	}

	static inline void next_row(void* context)
	{
		// nothing to do
	}

	static inline int dither(void* context, int pixel, int row, int column)
	{
		return pixel;
	}

	#include "pixel_proc_c_high_bit_depth_common.h"

};