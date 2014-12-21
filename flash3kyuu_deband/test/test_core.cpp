#include "stdafx.h"

#include <memory>
#include "gtest_stub.h"
#include <f3kdb.h>
#include "../src/compiler_compat.h"

using namespace testing;
using namespace std;

static const int TEST_ROUNDS = 2;

static const int GUARD_CONST = 0xDEADBEEF;

static unsigned char * create_guarded_buffer(int const height, int const pitch, unsigned char **data_start)
{
    assert(pitch % PLANE_ALIGNMENT == 0);
	unsigned char* buffer = (unsigned char*)_aligned_malloc( (height + 4) * pitch, PLANE_ALIGNMENT);

	for (int i = 0; i < pitch * 2; i += 4)
	{
		*(int*)(buffer + i) = GUARD_CONST;
	}
	for (int i = 0; i < pitch * 2; i += 4)
	{
		*(int*)(buffer + (height + 2) * pitch + i) = GUARD_CONST;
	}
	*data_start = buffer + pitch * 2;
	return buffer;
}

static void check_guard_bytes(unsigned char *buffer, int const height, int const pitch)
{
    {
        SCOPED_TRACE("Leading guard bytes was overwritten");
        for (int i = 0; i < pitch * 2; i += 4)
        {
            ASSERT_EQ(GUARD_CONST, *(int*)(buffer + i));
        }
    }
    {
        SCOPED_TRACE("Trailing guard bytes was overwritten");
        for (int i = 0; i < pitch * 2; i += 4)
        {
            ASSERT_EQ(GUARD_CONST, *(int*)(buffer + (height + 2) * pitch + i));
        }
    }
}

typedef struct _case_frame_t {
    f3kdb_video_info_t video_info;
    const unsigned char* frame_data;
} case_frame_t;

template <typename T>
class AlignedMemoryDeleter
{
public:
    void operator() (T*& ptr)
    {
        _aligned_free(ptr);
        ptr = nullptr;
    }
};

class F3kdbCoreDeleter
{
public:
    void operator() (f3kdb_core_t*& ptr)
    {
        if (!ptr) {
            return;
        }
        int ret = f3kdb_destroy(ptr);
        ptr = nullptr;
        assert(ret == F3KDB_SUCCESS);
    }
};

typedef unique_ptr< unsigned char, AlignedMemoryDeleter<unsigned char> > aligned_buffer_ptr;
typedef unique_ptr< f3kdb_core_t, F3kdbCoreDeleter > f3kdb_core_ptr;

class CoreTest : public TestWithParam< tuple<const char*, const case_frame_t*> > {
protected:
    virtual void SetUp() {
        int ret;
        ret = f3kdb_params_init_defaults(&_params);
        ASSERT_EQ(F3KDB_SUCCESS, ret);
        const char* param_string = nullptr;
        const case_frame_t* case_frame = nullptr;
        tie(param_string, case_frame) = GetParam();
        _frame_data = case_frame->frame_data;
        _video_info = case_frame->video_info;
        _video_info.num_frames = TEST_ROUNDS;
        ret = f3kdb_params_fill_by_string(&_params, param_string);
        ASSERT_EQ(F3KDB_SUCCESS, ret);
    }
public:
    CoreTest() : _frame_data(nullptr) {}
private:
    f3kdb_video_info_t _video_info;
    f3kdb_params_t _params;
    const unsigned char* _frame_data;

    int get_default_pitch(int width) {
        return (width + (PLANE_ALIGNMENT - 1)) & ~(PLANE_ALIGNMENT - 1);
    }

    void assert_eq_plane(const unsigned char* reference_plane, const unsigned char* test_plane, int pitch, int width, int height) {
        for (auto row = 0; row < height; row++) {
            auto offset = row * pitch;
            ASSERT_EQ(0, memcmp(reference_plane + offset, test_plane + offset, width));
        }
    }

    void test_plane_impl(const unsigned char* src_data, int src_pitch, f3kdb_core_t* core, int plane, aligned_buffer_ptr* buffer_out, const unsigned char** reference_data_start) {
        // true:  Compare output plane with reference plane
        // false: Build reference plane
        bool check_plane = !buffer_out;
        ASSERT_EQ(check_plane, !!*reference_data_start);

        int plane_height = _video_info.get_plane_height(plane);
        int plane_width = _video_info.get_plane_width(plane);
        int w_mul = _params.output_mode == HIGH_BIT_DEPTH_INTERLEAVED ? 2 : 1;
        int h_mul = _params.output_mode == HIGH_BIT_DEPTH_STACKED ? 2 : 1;
        int plane_height_raw = plane_height * h_mul;
        int plane_width_raw = plane_width * w_mul;
        int dst_pitch = get_default_pitch(plane_width_raw);
        ASSERT_GE(dst_pitch, plane_width_raw);
        aligned_buffer_ptr buffers[TEST_ROUNDS];
        unsigned char* start_ptrs[TEST_ROUNDS] = {nullptr};
        int ret;

        for (auto i = 0; i < TEST_ROUNDS; i++) {
            buffers[i].reset(create_guarded_buffer(plane_height_raw, dst_pitch, &start_ptrs[i]));
            ret = f3kdb_process_plane(core, i, plane, start_ptrs[i], dst_pitch, src_data, src_pitch);
            ASSERT_EQ(F3KDB_SUCCESS, ret);
            ASSERT_NO_FATAL_FAILURE(check_guard_bytes(buffers[i].get(), plane_height_raw, dst_pitch));
            if (check_plane) {
                assert_eq_plane(*reference_data_start, start_ptrs[i], dst_pitch, plane_width_raw, plane_height_raw);
            }
        }
        
        if (!check_plane) {
            // Make sure output plane from all invocations are the same 
            for (auto i = 1; i < TEST_ROUNDS; i++) {
                assert_eq_plane(start_ptrs[0], start_ptrs[i], dst_pitch, plane_width_raw, plane_height_raw);
            }
            buffer_out->swap(buffers[0]);
            *reference_data_start = start_ptrs[0];
        }
    }

    void prepare_src_data(int plane, aligned_buffer_ptr* buffer_out, const unsigned char** data_start_out, int* src_pitch_ptr, int alignment_offset = 0) {
        ASSERT_NE(nullptr, buffer_out);
        ASSERT_NE(nullptr, data_start_out);
        ASSERT_NE(nullptr, src_pitch_ptr);

        int plane_height = _video_info.get_plane_height(plane);
        int plane_width = _video_info.get_plane_width(plane);
        int w_mul = _video_info.pixel_mode == HIGH_BIT_DEPTH_INTERLEAVED ? 2 : 1;
        int h_mul = _video_info.pixel_mode == HIGH_BIT_DEPTH_STACKED ? 2 : 1;
        int plane_height_raw = plane_height * h_mul;
        int plane_width_raw = plane_width * w_mul;
        if (!*src_pitch_ptr) {
            *src_pitch_ptr = get_default_pitch(plane_width_raw);
        }
        ASSERT_GE(*src_pitch_ptr, plane_width_raw);
        buffer_out->reset((unsigned char*)_aligned_malloc(*src_pitch_ptr * plane_height_raw + alignment_offset, PLANE_ALIGNMENT));
        auto data_start = buffer_out->get() + alignment_offset;
        auto source_data = _frame_data;
        if (plane == PLANE_CB || plane == PLANE_CR) {
            source_data += _video_info.width * w_mul * _video_info.height * h_mul;
            if (plane == PLANE_CR) {
                source_data += plane_width_raw * plane_height_raw;
            }
        }

        for (auto i = 0; i < plane_height_raw; i++) {
            memcpy(data_start + *src_pitch_ptr * i, source_data + plane_width_raw * i, plane_width_raw);
        }

        *data_start_out = data_start;
    }


protected:
    void do_core_check(int alignment_offset = 0, int src_pitch_offset = 0) {
        f3kdb_core_ptr cores[IMPL_COUNT];
        static_assert(IMPL_C == 0, "We assumed IMPL_C == 0 here, fix it!");
        for (OPTIMIZATION_MODE opt = IMPL_C; opt < IMPL_COUNT; opt = (OPTIMIZATION_MODE)(opt + 1)) {
            _params.opt = opt;
            f3kdb_core_t* core_out = nullptr;
            char error_msg[2048];
            memset(error_msg, 0, sizeof(error_msg));
            int result = f3kdb_create(&_video_info, &_params, &core_out, error_msg, sizeof(error_msg) - 1);
            ASSERT_EQ(F3KDB_SUCCESS, result) << error_msg;
            ASSERT_NE(nullptr, core_out);
            cores[opt].reset(core_out);
        }
        const int planes[] = {PLANE_Y, PLANE_CB, PLANE_CR};
        char scoped_trace_text[2048];
        memset(scoped_trace_text, 0, sizeof(scoped_trace_text));
        for (int i = 0; i < (int)(sizeof(planes) / sizeof(planes[0])); i++) {
            int plane = planes[i];
            _snprintf(scoped_trace_text, sizeof(scoped_trace_text) - 1, "i = %d, plane = 0x%x", i, plane);
            SCOPED_TRACE(scoped_trace_text);

            int src_pitch = 0;
            aligned_buffer_ptr src_buffer;
            const unsigned char* src_data_start = nullptr;

            auto prepare_src = [&] {
                src_buffer.reset();
                src_data_start = nullptr;
                ASSERT_NO_FATAL_FAILURE(prepare_src_data(plane, &src_buffer, &src_data_start, &src_pitch, alignment_offset));
            };
            // We need to make sure src_pitch is calculated before checking src_pitch_offset
            ASSERT_NO_FATAL_FAILURE(prepare_src());
            ASSERT_GT(src_pitch, 0);

            if (src_pitch_offset) {
                src_pitch += src_pitch_offset;
                ASSERT_NO_FATAL_FAILURE(prepare_src());
            }

            auto run_test = [&] {
                aligned_buffer_ptr reference_buffer;
                const unsigned char* reference_data_start = nullptr;
                ASSERT_NO_FATAL_FAILURE(test_plane_impl(src_data_start, src_pitch, cores[IMPL_C].get(), plane, &reference_buffer, &reference_data_start));
                for (OPTIMIZATION_MODE opt = (OPTIMIZATION_MODE)(IMPL_C + 1); opt < IMPL_COUNT; opt = (OPTIMIZATION_MODE)(opt + 1)) {
                    _snprintf(scoped_trace_text, sizeof(scoped_trace_text) - 1, "opt = %d", opt);
                    SCOPED_TRACE(scoped_trace_text);
                    ASSERT_NO_FATAL_FAILURE(test_plane_impl(src_data_start, src_pitch, cores[opt].get(), plane, nullptr, &reference_data_start));
                }
            };
            {
                SCOPED_TRACE("Test #1");
                ASSERT_NO_FATAL_FAILURE(run_test());
            }
            {
                // Should work even after pitch changes
                SCOPED_TRACE("Test #2 ");
                src_pitch++;
                ASSERT_NO_FATAL_FAILURE(prepare_src());
                ASSERT_NO_FATAL_FAILURE(run_test());
            }
        }
    }

};

TEST_P(CoreTest, CoreCheckAligned) {
    do_core_check();
}

TEST_P(CoreTest, CoreCheckUnaligned) {
    do_core_check(1);
}

TEST_P(CoreTest, CoreCheckUnalignedPitch) {
    do_core_check(0, PLANE_ALIGNMENT - 1);
}

#include "test_core_param_set.h"

INSTANTIATE_TEST_CASE_P(Core, CoreTest, Combine(
    ValuesIn(param_set), ValuesIn(frames)
));