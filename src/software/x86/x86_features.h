#ifndef X86_FEATURES_H__
#define X86_FEATURES_H__

#define X86FEAT_MMX         (1 << 0)
// AMD, adds the SSE instructions that operate on MMX regs
#define X86FEAT_MMXEXT      (1 << 2)
#define X86FEAT_3DNOW       (1 << 3)
#define X86FEAT_3DNOWEXT    (1 << 4)
#define X86FEAT_SSE         (1 << 5)
#define X86FEAT_SSE2        (1 << 6)
#define X86FEAT_SSE3        (1 << 7)
#define X86FEAT_SSSE3       (1 << 8)
#define X86FEAT_SSE4_1      (1 << 9)
#define X86FEAT_SSE4_2      (1 <<10)
#define X86FEAT_AVX         (1 <<11)
#define X86FEAT_AVX2        (1 <<12)
#define X86FEAT_AVX512f     (1 <<13)
#define X86FEAT_AVX512dq    (1 <<14)
#define X86FEAT_AVX512ifma  (1 <<15)
#define X86FEAT_AVX512pf    (1 <<16)
#define X86FEAT_AVX512er    (1 <<17)
#define X86FEAT_AVX512cd    (1 <<18)
#define X86FEAT_AVX512bw    (1 <<19)
#define X86FEAT_AVX512vl    (1 <<20)
#define X86FEAT_AVX512vbmi  (1 <<21)
#define X86FEAT_PREFETCHWT1 (1 <<22)

uint64_t x86feat_get_features(void);


#endif
