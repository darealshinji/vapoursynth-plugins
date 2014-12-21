#pragma once

#include <math.h>

#include "impl_dispatch.h"

namespace pixel_proc_high_f_s_dithering {
    
// #define DUMP_DATA

    typedef unsigned short ERROR_TYPE;

    typedef struct _context_t
    {
        int output_depth;
        ERROR_TYPE* error_buffer;
        bool buffer_needs_dealloc;
        ERROR_TYPE* current_px_error;
        int row_pitch;
        int frame_width;
        int processed_pixels_in_current_line;
#ifdef DUMP_DATA
        FILE* debug_dump_fd[3];
#endif
    } context_t;

    static inline void init_context(char context_buffer[CONTEXT_BUFFER_SIZE], int frame_width, int output_depth)
    {
        context_t* ctx = (context_t*)context_buffer;
        int ctx_size = sizeof(context_t);
#if defined(NO_SSE) && defined(__INTEL_COMPILER)
        #pragma novector
        for (int i = 0; i < ctx_size; i++)
        {
            *( ( (unsigned char*)ctx ) + i ) = 0;
        }
#else
        // ICC will generate SSE code here
        memset(ctx, 0, ctx_size);
#endif

        // additional 2 bytes are placed at the beginning and the end
        int size_needed = (frame_width + 2) * 2 * sizeof(ERROR_TYPE);
        if (CONTEXT_BUFFER_SIZE - ctx_size < size_needed)
        {
            ctx->error_buffer = (ERROR_TYPE*)malloc(size_needed);
            ctx->buffer_needs_dealloc = true;
        } else {
            ctx->error_buffer = (ERROR_TYPE*)(context_buffer + ctx_size);
        }
        memset(ctx->error_buffer, 0, size_needed);
        ctx->current_px_error = ctx->error_buffer + 1;
        ctx->row_pitch = frame_width + 2;
        ctx->frame_width = frame_width;
        ctx->output_depth = output_depth;

#ifdef DUMP_DATA
        char file_name[256];
        sprintf(file_name, "fsdither_dump_stage0_%d", frame_width);
        ctx->debug_dump_fd[0] = fopen(file_name, "wb");
        sprintf(file_name, "fsdither_dump_stage1_%d", frame_width);
        ctx->debug_dump_fd[1] = fopen(file_name, "wb");
        sprintf(file_name, "fsdither_dump_stage2_%d", frame_width);
        ctx->debug_dump_fd[2] = fopen(file_name, "wb");
#endif
    }

    static inline void destroy_context(void* context)
    {
        context_t* ctx = (context_t*)context;
        if (ctx->buffer_needs_dealloc)
        {
            free(ctx->error_buffer);
            ctx->error_buffer = NULL;
        }
#ifdef DUMP_DATA
        for (int i = 0; i < sizeof(ctx->debug_dump_fd) / sizeof(FILE*); i++)
        {
            if (ctx->debug_dump_fd[i])
            {
                fclose(ctx->debug_dump_fd[i]);
            }
        }
#endif
    }

    static __forceinline void next_pixel(void* context)
    {
        context_t* ctx = (context_t*)context;
        ctx->current_px_error++;
        ctx->processed_pixels_in_current_line++;
    }

    static __forceinline void next_row(void* context)
    {
        context_t* ctx = (context_t*)context;
        ctx->row_pitch = -ctx->row_pitch;
        ctx->current_px_error = ctx->error_buffer + (ctx->row_pitch >> 31) * ctx->row_pitch;
        memset(ctx->current_px_error + ctx->row_pitch, 0, abs(ctx->row_pitch) * sizeof(ERROR_TYPE));
        ctx->current_px_error++;
        ctx->processed_pixels_in_current_line = 0;
    }

    static __forceinline int dither(void* context, int pixel, int row, int column);

    #include "pixel_proc_c_high_bit_depth_common.h"

    static const int PIXEL_MAX = ( ( 1 << (INTERNAL_BIT_DEPTH) ) - 1 );
    static const int PIXEL_MIN = 0;

    static __forceinline int dither(void* context, int pixel, int row, int column)
    {
        context_t* ctx = (context_t*)context;
        if (ctx->processed_pixels_in_current_line >= ctx->frame_width)
        {
            // outside plane, can occur in SSE code
            return pixel;
        }
#ifndef FS_DITHER_SKIP_PRE_CLAMP
        pixel = clamp_pixel(pixel, PIXEL_MIN, PIXEL_MAX);
#endif
#ifdef DUMP_DATA
        fwrite(&pixel, 4, 1, ctx->debug_dump_fd[0]);
#endif
        pixel += *(ctx->current_px_error);
#ifdef DUMP_DATA
        fwrite(&pixel, 4, 1, ctx->debug_dump_fd[1]);
#endif
        pixel = clamp_pixel(pixel, PIXEL_MIN, PIXEL_MAX);
#ifdef DUMP_DATA
        fwrite(&pixel, 4, 1, ctx->debug_dump_fd[2]);
#endif
        int new_error = pixel & ( ( 1 << (INTERNAL_BIT_DEPTH - ctx->output_depth) ) - 1 );
        *(ctx->current_px_error + 1) += (new_error * 7) >> 4;
        *(ctx->current_px_error + ctx->row_pitch - 1) += (new_error * 3) >> 4;
        *(ctx->current_px_error + ctx->row_pitch) += (new_error * 5) >> 4;
        *(ctx->current_px_error + ctx->row_pitch + 1) += (new_error * 1) >> 4;
        return pixel;
    }


};