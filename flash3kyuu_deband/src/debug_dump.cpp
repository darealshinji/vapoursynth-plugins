#include "debug_dump.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <Shlobj.h>

#include "compiler_compat.h"

#include <emmintrin.h>

#define DUMP_MAX_NAME_LENGTH 32
#define DUMP_MAX_STAGES 32

typedef struct _debug_dump_stage_t
{
    FILE* fd;
    int items_in_current_line;
    TCHAR name[DUMP_MAX_NAME_LENGTH + 1];
} debug_dump_stage_t;

typedef struct _debug_dump_t
{
    TCHAR dump_path[MAX_PATH];
    int items_per_line;
    debug_dump_stage_t stages[DUMP_MAX_STAGES];
} debug_dump_t;

const static TCHAR* DUMP_BASE_PATH = TEXT("%TEMP%\\f3kdb_dump\\");

#define FAIL_ON_ZERO(expr) if (!(expr)) abort()

alignas(4)
static volatile DWORD _tls_slot = TLS_OUT_OF_INDEXES;

void ensure_tls_slot()
{
    DWORD tls_slot = _tls_slot;
    if (tls_slot == TLS_OUT_OF_INDEXES)
    {
        tls_slot = TlsAlloc();
        if (InterlockedCompareExchange(&_tls_slot, tls_slot, TLS_OUT_OF_INDEXES) != TLS_OUT_OF_INDEXES)
        {
            // race failed, value initialized by other thread
            TlsFree(tls_slot);
        }
        assert(_tls_slot != TLS_OUT_OF_INDEXES);
    }

}

debug_dump_t* get_dump_handle(void)
{
    ensure_tls_slot();

    debug_dump_t* ret = (debug_dump_t*)TlsGetValue(_tls_slot);
    assert(GetLastError() == ERROR_SUCCESS);
    return ret;
}

debug_dump_t* create_dump_handle(void)
{
    ensure_tls_slot();

    debug_dump_t* ret = NULL;
    size_t size = sizeof(debug_dump_t);

    ret = (debug_dump_t*)malloc(size);
    memset(ret, 0, size);

    BOOL result = TlsSetValue(_tls_slot, ret);
    assert(result);

    return ret;
}


debug_dump_t* get_or_create_dump_handle(void)
{
    debug_dump_t* ret = get_dump_handle();

    if (!ret)
    {
        ret = create_dump_handle();
    }

    return ret;
}

void dump_init(const TCHAR* dump_base_name, int plane, int items_per_line)
{
    debug_dump_t* handle = get_or_create_dump_handle();

    handle->items_per_line = items_per_line;

    assert(handle);
    assert(dump_base_name);


    FAIL_ON_ZERO(ExpandEnvironmentStrings(DUMP_BASE_PATH, handle->dump_path, MAX_PATH));
    
    _tcscat(handle->dump_path, dump_base_name);

    TCHAR plane_str[4];
    memset(plane_str, 0, sizeof(plane_str));
    _sntprintf(plane_str, ARRAYSIZE(plane_str) - 1,  TEXT("%d"), plane);
    
    _tcscat(handle->dump_path, TEXT("_"));
    _tcscat(handle->dump_path, plane_str);
    _tcscat(handle->dump_path, TEXT("\\"));

    DWORD attributes = GetFileAttributes(handle->dump_path);

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        int ret = SHCreateDirectoryEx(NULL, handle->dump_path, NULL);
        if (ret != ERROR_SUCCESS)
        {
            abort();
        }
    } else {
        if ( (attributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY )
        {
            abort();
        }
    }
}

void dump_finish(void)
{
    debug_dump_t* handle = get_dump_handle();
    assert(handle);

    for (int i = 0; i < DUMP_MAX_STAGES; i++)
    {
        if (handle->stages[i].fd)
        {
            fclose(handle->stages[i].fd);
            memset(&(handle->stages[i]), 0, sizeof(debug_dump_stage_t));
        }
    }

    free(handle);
    TlsSetValue(_tls_slot, NULL);
}

void dump_next_line()
{
    debug_dump_t* handle = get_dump_handle();
    assert(handle);

    const int dummy_value = 0xdeadbeef;

    for (int i = 0; i < DUMP_MAX_STAGES; i++)
    {
        if (!handle->stages[i].fd)
        {
            return;
        }
        while (handle->stages[i].items_in_current_line < handle->items_per_line)
        {
            fwrite(&dummy_value, 4, 1, handle->stages[i].fd);
            handle->stages[i].items_in_current_line++;
        }
        handle->stages[i].items_in_current_line = 0;
    }
}

static debug_dump_stage_t* find_or_create_dump_stage(const TCHAR* dump_name)
{
    debug_dump_t* handle = get_dump_handle();

    assert(handle);
    assert(dump_name);
    assert(handle->dump_path[0] != 0);
    assert(_tcslen(dump_name) <= DUMP_MAX_NAME_LENGTH);
    
    for (int i = 0; i < DUMP_MAX_STAGES; i++)
    {
        if (!_tcscmp(dump_name, handle->stages[i].name))
        {
            return &handle->stages[i];
        }

        if (!handle->stages[i].fd)
        {
            // fd not found, create one
            TCHAR file_name[MAX_PATH];
            _tcscpy(file_name, handle->dump_path);
            _tcscat(file_name, dump_name);

            handle->stages[i].fd = _tfopen(file_name, TEXT("wb"));
            if (!handle->stages[i].fd)
            {
                abort();
            }
            _tcscpy(handle->stages[i].name, dump_name);
            return &handle->stages[i];
        }
    }

    // too many stages
    abort();
    return NULL;
}

void dump_value(const TCHAR* dump_name, int value)
{
    debug_dump_stage_t* stage = find_or_create_dump_stage(dump_name);
    debug_dump_t* handle = get_dump_handle();
    if (stage->items_in_current_line >= handle->items_per_line)
    {
        return;
    }
    fwrite(&value, 4, 1, stage->fd);
    stage->items_in_current_line++;
}

void dump_value(const TCHAR* dump_name, __m128i value, int word_size_in_bytes, bool is_signed)
{
    assert(word_size_in_bytes == 1 || word_size_in_bytes == 2 || word_size_in_bytes == 4);
    
    debug_dump_stage_t* stage = find_or_create_dump_stage(dump_name);
    debug_dump_t* handle = get_dump_handle();

    alignas(16)
    char buffer[16];

    _mm_store_si128((__m128i*)buffer, value);

    int item;
    for (int i = 0; i < 16 / word_size_in_bytes; i++)
    {
        if (stage->items_in_current_line >= handle->items_per_line)
        {
            return;
        }
        item = 0;
        memcpy(&item, buffer + i * word_size_in_bytes, word_size_in_bytes);
        if (is_signed)
        {
            int bits = (4 - word_size_in_bytes) * 8;
            item <<= bits;
            item >>= bits;
        }
        fwrite(&item, 4, 1, stage->fd);
        stage->items_in_current_line++;
    }
}
