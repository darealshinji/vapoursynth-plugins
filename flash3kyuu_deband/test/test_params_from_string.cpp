#include "stdafx.h"

#include <type_traits>

#include "gtest_stub.h"

#include <f3kdb.h>

using namespace testing;
using namespace std;

static_assert(is_same<unsigned short, decltype(((f3kdb_params_t*)nullptr)->Y)>::value, "f3kdb_params_t::Y should be unsigned short, or tests need to be fixed.");

TEST(ParamsFromStringTest, SingleNumber) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    params.Y = -1;
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "Y=42");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(params.Y, 42);
}

TEST(ParamsFromStringTest, NameShouldBeCaseInsensitive) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    params.Y = -1;
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=42");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(params.Y, 42);
}

TEST(ParamsFromStringTest, ShouldFailOnInvalidNumber) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=dummy42");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=42dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
}

TEST(ParamsFromStringTest, ShouldFailOnOverflow) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=-1");
    ASSERT_EQ(F3KDB_ERROR_VALUE_OUT_OF_RANGE, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=65536");
    ASSERT_EQ(F3KDB_ERROR_VALUE_OUT_OF_RANGE, ret);
}

TEST(ParamsFromStringTest, Double) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "random_param_grain=5.5");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(params.random_param_grain, 5.5);
}

TEST(ParamsFromStringTest, Bool) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=true");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_TRUE(params.keep_tv_range);
    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=1");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_TRUE(params.keep_tv_range);
    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=false");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_FALSE(params.keep_tv_range);
    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=0");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_FALSE(params.keep_tv_range);

    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=42");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
    ret = f3kdb_params_fill_by_string(&params, "keep_tv_range=dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
}

TEST(ParamsFromStringTest, Enum) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "dither_algo=2");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(DA_HIGH_ORDERED_DITHERING, params.dither_algo);
}

TEST(ParamsFromStringTest, MultiValues) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb=2:cr=4,keep_tv_range=true/random_param_grain=5.5");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(1, params.Y);
    ASSERT_EQ(2, params.Cb);
    ASSERT_EQ(4, params.Cr);
    ASSERT_TRUE(params.keep_tv_range);
    ASSERT_EQ(params.random_param_grain, 5.5);
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb=2,:cr=4,/keep_tv_range=true:/,random_param_grain=5.5/,");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(1, params.Y);
    ASSERT_EQ(2, params.Cb);
    ASSERT_EQ(4, params.Cr);
    ASSERT_TRUE(params.keep_tv_range);
    ASSERT_EQ(params.random_param_grain, 5.5);
}

TEST(ParamsFromStringTest, MultiValuesFailUnexpectedEnd) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb=2:cr");
    ASSERT_EQ(F3KDB_ERROR_UNEXPECTED_END, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb:cr=2");
    ASSERT_EQ(F3KDB_ERROR_UNEXPECTED_END, ret);
}

TEST(ParamsFromStringTest, MultiValuesFailInvalidValue) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_by_string(&params, "y=dummy/cb=2:cr=4");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb=dummy:cr=4");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
    ret = f3kdb_params_fill_by_string(&params, "y=1/cb=2:cr=dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_VALUE, ret);
}

TEST(ParamsFromStringTest, Preset) {
    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);
    int ret;
    ret = f3kdb_params_fill_preset(&params, "dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_NAME, ret);
    ret = f3kdb_params_fill_preset(&params, "depth/dummy");
    ASSERT_EQ(F3KDB_ERROR_INVALID_NAME, ret);
    ret = f3kdb_params_fill_preset(&params, "dummy/depth");
    ASSERT_EQ(F3KDB_ERROR_INVALID_NAME, ret);

    params.seed = 0xDEADBEEF;
    ret = f3kdb_params_fill_preset(&params, "depth");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_EQ(0, params.Y);
    ASSERT_EQ(0, params.Cb);
    ASSERT_EQ(0, params.Cr);
    ASSERT_EQ(0, params.grainY);
    ASSERT_EQ(0, params.grainC);

    ret = f3kdb_params_fill_preset(&params, "depth");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ret = f3kdb_params_fill_preset(&params, "low");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_NE(0, params.Y);
    ASSERT_NE(0, params.Cb);
    ASSERT_NE(0, params.Cr);
    ASSERT_NE(0, params.grainY);
    ASSERT_NE(0, params.grainC);

    f3kdb_params_init_defaults(&params);
    ASSERT_NE(0, params.Y);
    params.seed = 0xDEADBEEF;
    ret = f3kdb_params_fill_preset(&params, "nograin");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_NE(0, params.Y);
    ASSERT_EQ(0, params.grainY);
    ASSERT_EQ(0, params.grainC);

    ret = f3kdb_params_fill_preset(&params, "depth");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ret = f3kdb_params_fill_preset(&params, "low/nograin");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_NE(0, params.Y);
    ASSERT_NE(0, params.Cb);
    ASSERT_NE(0, params.Cr);
    ASSERT_EQ(0, params.grainY);
    ASSERT_EQ(0, params.grainC);

    ret = f3kdb_params_fill_preset(&params, "nograin/low");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_NE(0, params.Y);
    ASSERT_NE(0, params.Cb);
    ASSERT_NE(0, params.Cr);
    ASSERT_NE(0, params.grainY);
    ASSERT_NE(0, params.grainC);

    ret = f3kdb_params_fill_preset(&params, "high/luma");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_NE(0, params.Y);
    ASSERT_EQ(0, params.Cb);
    ASSERT_EQ(0, params.Cr);
    ASSERT_NE(0, params.grainY);
    ASSERT_EQ(0, params.grainC);

    ret = f3kdb_params_fill_preset(&params, "high/chroma");
    ASSERT_EQ(F3KDB_SUCCESS, ret);
    ASSERT_EQ(0xDEADBEEF, params.seed);
    ASSERT_EQ(0, params.Y);
    ASSERT_NE(0, params.Cb);
    ASSERT_NE(0, params.Cr);
    ASSERT_EQ(0, params.grainY);
    ASSERT_NE(0, params.grainC);
}