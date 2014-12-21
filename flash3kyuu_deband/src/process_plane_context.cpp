#include "stdafx.h"

#include "process_plane_context.h"

#include <cstring>
#include <assert.h>

// disable SSE instructions here to allow running on pre-SSE2 systems
#if defined(__INTEL_COMPILER)

void safe_zero_memory(void* start_address, size_t count)
{
    char* ptr = (char*)start_address;
#pragma novector
    for (int i = 0; i < count; i++)
    {
        *(ptr + i) = 0;
    }
}

#else

#define safe_zero_memory(start_address, count) memset(start_address, 0, count)

#endif

void destroy_context(process_plane_context* context)
{
    assert(context);

    if (context->data) {
        assert(context->destroy);
        context->destroy(context->data);
        safe_zero_memory(context, sizeof(process_plane_context));
    }
}

void init_context(process_plane_context* context)
{
    assert(context);
    safe_zero_memory(context, sizeof(process_plane_context));
}
