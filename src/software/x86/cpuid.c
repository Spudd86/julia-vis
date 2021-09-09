#if (__x86_64__ || __i386__)  && !defined(DISABLE_X86_INTRIN)

#include "common.h"
#include "x86_features.h"

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

// static inline unsigned long read_cr0(void)
// {
// 	unsigned long val;
// 	__asm__ volatile ( "mov %%cr0, %0" : "=r"(val) );
// 	return val;
// }

// static inline unsigned long read_cr4(void)
// {
//     unsigned long val;
//     __asm__ volatile ( "mov %%cr4, %0" : "=r"(val) );
//     return val;
// }
#endif

uint64_t x86feat_get_features(void)
{
	uint64_t features = 0;

	//TODO: environment variable to force disable detecting various features
	//TODO: detect OS support for features (mostly newer ones...)
	bool os_xsave = false;
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	if (!cpuid (1, &eax, &ebx, &ecx, &edx)) return 0;

	// check OSFXSR, MP and EM flags, need OSFXSR and MP to be 1 and EM to be 0 or sse instructions will cause illegal instruction
	// unfortunately there doesn't seem to be a way to get at any of that information outside ring 0

	os_xsave = !!(ecx & (1 << 27)); // os using the XSAVE to save context

	if(edx & (1 << 23)) features |= X86FEAT_MMX;
	if(edx & (1 << 25)) features |= X86FEAT_MMXEXT; // MMXEXT is a subset of SSE, AMD only though
	if(edx & (1 << 24)) { // fxsr os support needed
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
