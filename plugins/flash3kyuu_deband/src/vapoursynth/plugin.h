#pragma once

#include <type_traits>
#include <limits>
#include <stdio.h>
#include <memory.h>

#include <f3kdb.h>
#include "VapourSynth.h"
#include "../compiler_compat.h"

static const int _peOutOfRange = 0x7fffffff;
static const int _peNoError = 0;

template <class T>
static T get_value_from_vsmap(const VSAPI* vsapi, const VSMap* in, const char* name, int* error)
{
    using namespace std;

    int64_t raw_int = vsapi->propGetInt(in, name, 0, error);
    if (*error == _peNoError && !is_enum<T>::value && (raw_int < numeric_limits<T>::min() || raw_int > numeric_limits<T>::max()))
    {
        *error = _peOutOfRange;
        return (T)0;
    }
    return (T)raw_int;
}

template <>
inline bool get_value_from_vsmap<bool>(const VSAPI* vsapi, const VSMap* in, const char* name, int* error)
{
    return !!vsapi->propGetInt(in, name, 0, error);
}

template <>
inline double get_value_from_vsmap<double>(const VSAPI* vsapi, const VSMap* in, const char* name, int* error)
{
    return vsapi->propGetFloat(in, name, 0, error);
}

template <class T>
inline bool param_from_vsmap(T* target, const char* name, const VSMap* in, VSMap* out,  const VSAPI* vsapi)
{
    using namespace std;

    static_assert(is_integral<T>::value || is_same<T, double>::value || is_enum<T>::value, "Unsupported type");

    int error = _peNoError;

    auto _set_error = [=](const char* message) {
        char error_msg[1024];
        memset(error_msg, 0, sizeof(error_msg));
        _snprintf(error_msg, sizeof(error_msg) - 1, message, name, error);
        vsapi->setError(out, error_msg);
    };

    T value = get_value_from_vsmap<T>(vsapi, in, name, &error);
    switch (error)
    {
    case _peNoError:
        break;
    case _peOutOfRange:
        _set_error("f3kdb: Argument out of range: %s");
        return false;
    case peUnset:
        // Optional parameter
        return true;
    case peType:
        _set_error("f3kdb: Invalid argument type: %s");
        return false;
    default:
        _set_error("f3kdb: Unknown argument error: %s (code %d)");
        return false;
        break;
    }
    *target = value;
    return true;
}