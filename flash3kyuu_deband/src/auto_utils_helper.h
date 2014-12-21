#pragma once

#include <type_traits>
#include <limits>
#include <cerrno>
#include <stdlib.h>
#include <stdarg.h>

#include <f3kdb.h>

using namespace std;

template<class T>
struct number_converter
{
    typedef long intermediate_type;
    static intermediate_type convert(const char* value_string, char** end)
    {
        return strtol(value_string, end, 10);
    }
};

template<>
struct number_converter<double>
{
    typedef double intermediate_type;
    static intermediate_type convert(const char* value_string, char** end)
    {
        return strtod(value_string, end);
    }
};

template<typename T>
int params_set_value_by_string(T* target, const char* value_string)
{
    static_assert(is_integral<T>::value || is_same<T, double>::value || is_enum<T>::value, "T must be integral type");
    char* end = NULL;
    errno = 0;
    typename number_converter<T>::intermediate_type value;
    value = number_converter<T>::convert(value_string, &end);
    if (errno == ERANGE)
    {
        return F3KDB_ERROR_VALUE_OUT_OF_RANGE;
    }
    if (end != value_string + strlen(value_string))
    {
        return F3KDB_ERROR_INVALID_VALUE;
    }
    if (is_integral<T>::value)
    {
        if (value < numeric_limits<T>::min() || value > numeric_limits<T>::max())
        {
            return F3KDB_ERROR_VALUE_OUT_OF_RANGE;
        }
    }
    *target = (T)value;
    return F3KDB_SUCCESS;
}

bool any_equals(const char* comparand, ...)
{
    va_list va;
    va_start(va, comparand);
    const char* str = va_arg(va, const char*);
    while (str)
    {
        if (!_stricmp(comparand, str))
        {
            va_end(va);
            return true;
        }
        str = va_arg(va, const char*);
    }
    va_end(va);
    return false;
}

template<>
int params_set_value_by_string<bool>(bool* target, const char* value_string)
{
    if (any_equals(value_string, "true", "t", "yes", "y", "1", NULL))
    {
        *target = true;
        return F3KDB_SUCCESS;
    }
    if (any_equals(value_string, "false", "f", "no", "n", "0", NULL))
    {
        *target = false;
        return F3KDB_SUCCESS;
    }
    return F3KDB_ERROR_INVALID_VALUE;
}