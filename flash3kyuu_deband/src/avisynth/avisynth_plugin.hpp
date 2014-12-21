#include "stdafx.h"
#include "avisynth.h"

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define Create_flash3kyuu_deband CONCAT(Create_flash3kyuu_deband_v, USE_AVISYNTH_INTERFACE)
#define f3kdb_avisynth CONCAT(f3kdb_avisynth_v, USE_AVISYNTH_INTERFACE)

#include "../compiler_compat.h"
#include "filter_impl.hpp"

#if USE_AVISYNTH_INTERFACE == 3
F3KDB_API(const char*) AvisynthPluginInit2(IScriptEnvironment* env)
{
#elif USE_AVISYNTH_INTERFACE == 5
const AVS_Linkage *AVS_linkage;

F3KDB_API(const char*) AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {
    AVS_linkage = vectors;
#endif
	env->AddFunction("flash3kyuu_deband", 
		F3KDB_AVS_PARAMS, 
		Create_flash3kyuu_deband, 
		NULL);
	env->AddFunction("f3kdb", 
		F3KDB_AVS_PARAMS, 
		Create_flash3kyuu_deband, 
		NULL);

	return "f3kdb";
}