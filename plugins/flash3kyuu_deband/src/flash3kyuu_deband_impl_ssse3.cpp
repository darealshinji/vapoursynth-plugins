#include "stdafx.h"

#include <tmmintrin.h>

#define SSE_LIMIT 31

#include "flash3kyuu_deband_sse_base.h"

#define DECLARE_IMPL_SSSE3
#include "impl_dispatch_decl.h"
