#include "stdafx.h"

#include <stdio.h>

#ifdef ENABLE_DEBUG_DUMP

#include <emmintrin.h>

void dump_init(const TCHAR* dump_base_name, int plane, int items_per_line);

void dump_next_line();

void dump_value(const TCHAR* dump_name, int value);

void dump_value(const TCHAR* dump_name, __m128i value, int word_size_in_bytes, bool is_signed);

void dump_finish();

#define DUMP_INIT(name, plane, items_per_line) dump_init( TEXT(name), plane, items_per_line )

#define DUMP_NEXT_LINE() dump_next_line()

#define DUMP_VALUE(name, ...) dump_value(TEXT(name), __VA_ARGS__)

#define DUMP_VALUE_S(name, ...) dump_value(name, __VA_ARGS__)

#define DUMP_FINISH() dump_finish()

#else

#define DUMP_INIT(name, ...) ((void)0)

#define DUMP_NEXT_LINE() ((void)0)

#define DUMP_VALUE(name, ...) ((void)0)

#define DUMP_VALUE_S(name, ...) ((void)0)

#define DUMP_FINISH() ((void)0)

#endif
