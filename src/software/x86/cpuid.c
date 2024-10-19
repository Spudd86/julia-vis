#if (__x86_64__ || __i386__)  && !defined(DISABLE_X86_INTRIN)

#include "common.h"
#include "x86_features.h"

#include <signal.h>
#include <setjmp.h>

#ifdef _WIN32
#include <intrin.h>
int cpuid(unsigned int level, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
	unsigned int info[4];

	// check support first
	unsigned int ext = level & 0x80000000;
	__cpuidex(info, ext, 0);
	if(info[0] < level) return 0;

	__cpuidex(info, level, 0);
	*eax = info[0]
	*ebx = info[1];
	*ecx = info[2];
	*edx = info[3];

	return 1;
}
#else
#include <cpuid.h>
int cpuid(unsigned int level, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
	unsigned int ext = level & 0x80000000;

	if (__get_cpuid_max(ext, 0) < level) return 0;

	__cpuid_count(level, 0, *eax, *ebx, *ecx, *edx);

	return 1;
}

static sigjmp_buf sse_sigjmp_buf;
static void handle_SIGILL_for_sse_check(int signal) { siglongjmp(sse_sigjmp_buf, 1); }

#endif

uint64_t x86feat_get_features(void)
{
	uint64_t features = 0;

	//TODO: environment variable to force disable detecting various features
	//TODO: detect OS support for features (mostly newer ones...)
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	if (!cpuid (1, &eax, &ebx, &ecx, &edx)) return 0;

	// check OSFXSR, MP and EM flags, need OSFXSR and MP to be 1 and EM to be 0 or sse instructions will cause illegal instruction
	// unfortunately there doesn't seem to be a way to get at any of that information outside ring 0

	// Try to run stmxcsr instruction, if OS doesn't support SSE we will get an illegal instruction exception
	bool os_fxsr = false;
#if defined(WIN32)
	__try {
		uint32_t mxcsr = 0;
		asm("stmxcsr %0"::"m"(mxcsr) : );
		os_fxsr = true;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
#else
	struct sigaction orig_action;
	struct sigaction new_action = {{0}};
	new_action.sa_handler = handle_SIGILL_for_sse_check;
	new_action.sa_flags = 0;
	sigemptyset(&new_action.sa_mask);
	sigaction(SIGILL, &new_action, &orig_action);
	if(!sigsetjmp(sse_sigjmp_buf, 1))
	{
		uint32_t mxcsr = 0;
		asm("stmxcsr %0"::"m"(mxcsr) : );
		os_fxsr = true;
	}
	sigaction(SIGILL, &orig_action, NULL);
#endif

	bool os_xsave = !!(ecx & (1 << 27)); // os using the XSAVE to save context

	if(edx & (1 << 23)) features |= X86FEAT_MMX;
	if(edx & (1 << 25)) features |= X86FEAT_MMXEXT; // MMXEXT is a subset of SSE, AMD only though
	if( (edx & (1 << 24)) && os_fxsr ) { // fxsr os support needed
	    if(edx & (1 << 25)) features |= X86FEAT_SSE;
	    if(edx & (1 << 26)) features |= X86FEAT_SSE2;
	    if(ecx & (1 <<  0)) features |= X86FEAT_SSE3;
	    if(ecx & (1 <<  9)) features |= X86FEAT_SSSE3;
		if(ecx & (1 << 19)) features |= X86FEAT_SSE4_1;
		if(ecx & (1 << 20)) features |= X86FEAT_SSE4_2;
		if((ecx & (1 << 28)) && os_xsave) features |= X86FEAT_AVX; // TODO: check XGETBV to see if OS is saving AVX regs
	}


	if(cpuid(7, &eax, &ebx, &ecx, &edx)) {
		// TODO: check XGETBV to see if OS is saving AVX regs
		if((ebx & (1 <<  5)) && os_xsave) features |= X86FEAT_AVX2;
		if((ebx & (1 << 16)) && os_xsave) features |= X86FEAT_AVX512f;
		if((ebx & (1 << 17)) && os_xsave) features |= X86FEAT_AVX512dq;
		if((ebx & (1 << 21)) && os_xsave) features |= X86FEAT_AVX512ifma;
		if((ebx & (1 << 26)) && os_xsave) features |= X86FEAT_AVX512pf;
		if((ebx & (1 << 27)) && os_xsave) features |= X86FEAT_AVX512er;
		if((ebx & (1 << 28)) && os_xsave) features |= X86FEAT_AVX512cd;
		if((ebx & (1 << 30)) && os_xsave) features |= X86FEAT_AVX512bw;
		if((ebx & (1 << 31)) && os_xsave) features |= X86FEAT_AVX512vl;
		if((ecx & (1 <<  1)) && os_xsave) features |= X86FEAT_AVX512vbmi;

		if(ecx & (1 <<  0)) features |= X86FEAT_PREFETCHWT1;
	}

	//__get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	//if(eax<0x80000001) return features;
	if(__get_cpuid_max(0x80000000, NULL) < 0x80000001u) return features;

	__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	if(edx & (1 << 30)) features |= X86FEAT_3DNOWEXT;
	if(edx & (1 << 31)) features |= X86FEAT_3DNOW;
	if(edx & (1 << 22)) features |= X86FEAT_MMXEXT; // AMD added the extra mmx instructions without doing all of sse, we only need the ones in both places
	if(edx & (1 << 23)) features |= X86FEAT_MMX;

	return features;
}

#endif
