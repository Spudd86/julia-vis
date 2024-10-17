
#include "common.h"
#include "pixmisc.h"

//TODO: include a way to force a particular version

//TODO: paratask!
//      need to find a line split that works for alignment for SIMD versions


typedef void (*maxblend_fn)(void *restrict dest, const void *restrict src, size_t n);



#if defined(HAVE_ORC)
#include <orc/orc.h>
#include <threads.h>

static OrcProgram *maxblend_orc_program = NULL;

static void maxblend_orc_init(void)
{
	maxblend_orc_program = orc_program_new_ds(2,2);
	orc_program_append_str(maxblend_orc_program , "maxuw", "d1", "d1", "s1");
	orc_program_compile (maxblend_orc_program );  //TODO: check return value here
}

void blend(void *restrict dest, const void *restrict src, size_t n)
{
	static once_flag flag = ONCE_FLAG_INIT;
	call_once(&flag, maxblend_orc_init);

	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	orc_executor_set_program (ex, maxblend_orc_program);
	orc_executor_set_n (ex, n);
	orc_executor_set_array (ex, ORC_VAR_S1, src);
	orc_executor_set_array (ex, ORC_VAR_D1, dest);
	orc_executor_run (ex);
}

#elif (__x86_64__ || __i386__) && !defined(DISABLE_X86_INTRIN)

#include "x86/x86_features.h"

static void maxblend_dispatch(void *restrict dest, const void *restrict src, size_t n);

static maxblend_fn blend = maxblend_dispatch;
static int simd_width = 64;

static void maxblend_dispatch(void *restrict dest, const void *restrict src, size_t n)
{
	blend = maxblend_fallback;

	uint64_t feat = x86feat_get_features();

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) blend = maxblend_mmx;
	if(feat & X86FEAT_MMXEXT) blend = maxblend_3dnow;
#endif
	if(feat & X86FEAT_SSE) blend = maxblend_sse;
	if(feat & X86FEAT_SSE2) blend = maxblend_sse2;
	if(feat & X86FEAT_SSE4_1) blend = maxblend_sse4_1;
#ifndef __EMSCRIPTEN__
	if(feat & X86FEAT_AVX2) blend = maxblend_avx2;
#endif
	blend(dest, src, n);
}

#else
#define maxblend_fallback blend
#endif

__attribute__((hot))
void maxblend_fallback(void *restrict dest, const void *restrict src, size_t n)
{
	uint16_t *restrict d=dest; const uint16_t *restrict s=src;
	for(size_t i=0; i<n; i++)
		d[i] = MAX(d[i], s[i]);
}

// #define NO_PARATASK 1

#if NO_PARATASK

void maxblend(void *restrict dest, const void *restrict src, int w, int h) {
	blend(dest, src, (size_t)w*(size_t)h);
}

#else

#include "paratask/paratask.h"

struct task_args {
	void *restrict dest;
	const void *restrict src;
	int w, h;
	int span;
};

static void maxblend_paratask_func(size_t work_item_id, void *arg_)
{
	struct task_args *a = arg_;
	size_t offset = work_item_id * a->span * a->w * sizeof(uint16_t);

	size_t end = MIN((work_item_id + 1) * a->span * a->w, a->w * a->h);
	size_t count = end - work_item_id * a->span * a->w;

	blend(a->dest + offset, a->src + offset, count);
}

void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	// Want span st w*span is divsible by alignment requirements of SIMD
	int task_span = 1;
	while( 0 != ((w*task_span)%16) ) task_span++; // widest SIMD we use is avx2 at 32 bytes, or 16 pixels

	struct task_args args = {
		dest, src,
		w, h,
		task_span
	};
	paratask_call(paratask_default_instance(), 0, h/task_span, maxblend_paratask_func, &args);
}

#endif
