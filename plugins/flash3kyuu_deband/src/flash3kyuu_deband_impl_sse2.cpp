#include "stdafx.h"

#include <emmintrin.h>

#define SSE_LIMIT 20

#include "flash3kyuu_deband_sse_base.h"

#define DECLARE_IMPL_SSE2
#include "impl_dispatch_decl.h"
