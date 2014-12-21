#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma clang diagnostic pop
#else
#include <gtest/gtest.h>
#endif