#if (__x86_64__ || __i386__)

#include "common.h"
#include "x86_features.h"
#include <cpuid.h>

#ifndef bit_SSE41
#define bit_SSE41       0x00080000
#endif

uint64_t x86feat_get_features(void)
{
	uint64_t features = 0;

	//TODO: environment variable to force disable detecting various features

	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    if(edx & (1 << 23)) features |= X86FEAT_MMX;
    if(edx & (1 << 25)) features |= X86FEAT_SSE;
    if(edx & (1 << 25)) features |= X86FEAT_SSE2;

    if(ecx & (1 <<  0)) features |= X86FEAT_SSE3;
    if(ecx & (1 <<  9)) features |= X86FEAT_SSSE3;
	if(ecx & (1 << 19)) features |= X86FEAT_SSE4_1;
	if(ecx & (1 << 20)) features |= X86FEAT_SSE4_2;
	if(ecx & (1 << 28)) features |= X86FEAT_AVX;

	__get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	if(eax<0x80000001) return features;

	__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	if(edx & (1 << 30)) features |= X86FEAT_3DNOWEXT;
	if(edx & (1 << 31)) features |= X86FEAT_3DNOW;
	if(edx & (1 << 22)) features |= X86FEAT_MMXEXT; // AMD added the extra mmx instructions without doing all of sse, we only need the ones in both places
	if(edx & (1 << 23)) features |= X86FEAT_MMX;

	return features;
}

#endif
