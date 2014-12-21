
#ifdef __INTEL_COMPILER
extern "C" void ___intel_cpu_indicator_init();
#else
#define ___intel_cpu_indicator_init() 
#endif