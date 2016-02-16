#include "stdafx.h"

#include "core.h"

#define IMPL_DISPATCH_IMPORT_DECLARATION

#include "impl_dispatch_decl.h"

const process_plane_impl_t* process_plane_impl_high_precision_no_dithering[] = {
	process_plane_impl_c_high_no_dithering,
	process_plane_impl_sse2_high_no_dithering,
	process_plane_impl_ssse3_high_no_dithering,
	process_plane_impl_sse4_high_no_dithering
};

const process_plane_impl_t* process_plane_impl_high_precision_ordered_dithering[] = {
	process_plane_impl_c_high_ordered_dithering,
	process_plane_impl_sse2_high_ordered_dithering,
	process_plane_impl_ssse3_high_ordered_dithering,
	process_plane_impl_sse4_high_ordered_dithering
};

const process_plane_impl_t* process_plane_impl_high_precision_floyd_steinberg_dithering[] = {
	process_plane_impl_c_high_floyd_steinberg_dithering,
	process_plane_impl_sse2_high_floyd_steinberg_dithering,
	process_plane_impl_ssse3_high_floyd_steinberg_dithering,
	process_plane_impl_sse4_high_floyd_steinberg_dithering
};

const process_plane_impl_t* process_plane_impl_16bit_stacked[] = {
	process_plane_impl_c_16bit_stacked,
	process_plane_impl_sse2_16bit_stacked,
	process_plane_impl_ssse3_16bit_stacked,
	process_plane_impl_sse4_16bit_stacked
};

const process_plane_impl_t* process_plane_impl_16bit_interleaved[] = {
	process_plane_impl_c_16bit_interleaved,
	process_plane_impl_sse2_16bit_interleaved,
	process_plane_impl_ssse3_16bit_interleaved,
	process_plane_impl_sse4_16bit_interleaved
};


const process_plane_impl_t** process_plane_impls[] = {
	nullptr, // process_plane_impl_low_precision has been removed,
	process_plane_impl_high_precision_no_dithering,
	process_plane_impl_high_precision_ordered_dithering,
	process_plane_impl_high_precision_floyd_steinberg_dithering,
    process_plane_impl_16bit_stacked,
    process_plane_impl_16bit_interleaved
};