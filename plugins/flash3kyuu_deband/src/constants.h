#pragma once

// alignment for SSE operations
#define FRAME_LUT_ALIGNMENT 16

#define INTERNAL_BIT_DEPTH 16

// these range values are defined in internal bit depth
#define TV_RANGE_Y_MIN (16 << (INTERNAL_BIT_DEPTH - 8))
#define TV_RANGE_Y_MAX (235 << (INTERNAL_BIT_DEPTH - 8))

#define TV_RANGE_C_MIN TV_RANGE_Y_MIN
#define TV_RANGE_C_MAX (240 << (INTERNAL_BIT_DEPTH - 8))

#define FULL_RANGE_Y_MIN 0
#define FULL_RANGE_Y_MAX ((1 << INTERNAL_BIT_DEPTH) - 1)

#define FULL_RANGE_C_MIN FULL_RANGE_Y_MIN
#define FULL_RANGE_C_MAX FULL_RANGE_Y_MAX

#define VALUE_8BIT(x) ( x >> ( INTERNAL_BIT_DEPTH - 8 ) )

