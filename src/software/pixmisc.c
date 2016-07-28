
#include "common.h"
#include "pixmisc.h"

//TODO: include a way to force a particular version

#if defined(HAVE_ORC)
#include <orc/orc.h>
void maxblend(void *restrict dest, const void *restrict src, int w, int h)
{
	static OrcProgram *p = NULL;
	if (p == NULL) {
		p = orc_program_new_ds(2,2);
		orc_program_append_str(p, "maxuw", "d1", "d1", "s1");
		orc_program_compile (p);  //TODO: check return value here
	}

	OrcExecutor _ex;
	OrcExecutor *ex = &_ex;
	orc_executor_set_program (ex, p);
	orc_executor_set_n (ex, w*h);
	orc_executor_set_array (ex, ORC_VAR_S1, src);
	orc_executor_set_array (ex, ORC_VAR_D1, dest);
	orc_executor_run (ex);
}

#elif (__x86_64__ || __i386__)

#include "x86/x86_features.h"

static void maxblend_dispatch(void *restrict dest, const void *restrict src, int w, int h);

typedef void (*maxblend_fn)(void *restrict dest, const void *restrict src, int w, int h);

static maxblend_fn blend = maxblend_dispatch;

static void maxblend_dispatch(void *restrict dest, const void *restrict src, int w, int h)
{
	blend = maxblend_fallback;

	uint64_t feat = x86feat_get_features();

#if !defined(__x86_64__)
	if(feat & X86FEAT_MMX) blend = maxblend_mmx;
	if(feat & X86FEAT_MMXEXT) blend = maxblend_sse; // AMD added the extra mmx instructions without doing all of sse, we only need the ones in both places
	if(feat & X86FEAT_SSE) blend = maxblend_sse;
#endif
	if(feat & X86FEAT_SSE2) blend = maxblend_sse2;
	if(feat & X86FEAT_SSE4_1) blend = maxblend_sse4_1;

	blend(dest, src, w, h);
}

void maxblend(void *restrict dest, const void *restrict src, int w, int h) {
	blend(dest, src, w, h);
}

#else
#define maxblend_fallback maxblend
#endif

void maxblend_fallback(void *restrict dest, const void *restrict src, int w, int h)
{
	const int n = w*h;
	uint16_t *restrict d=dest; const uint16_t *restrict s=src;
	for(int i=0; i<n; i++)
		d[i] = MAX(d[i], s[i]);
}

