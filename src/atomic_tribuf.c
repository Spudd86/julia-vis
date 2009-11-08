
#include "common.h"
#include "tribuf.h"

#include <stdio.h>

#define tribuf_error(s) do {\
		fflush(stdout); \
		fprintf(stderr, "ERROR: %s:%i ", __FILE__, __LINE__); \
		fprintf(stderr, "%s\n", s); fflush(stderr); \
		abort();\
	} while(0)
#ifdef TB_DEBUG
#define tb_sanity_check(c, m) do {\
		if((c)) tribuf_error((m));\
	} while(0)
#else
#define tb_sanity_check(c, m)
#endif

#if (defined(TRIBBUF_PROFILE) || defined(TB_DEBUG)) && defined(HAVE_ATEXIT)
static unsigned int finish_writes = 0;
static unsigned int finish_write_trys = 0;
static unsigned int finish_reads = 0;
static unsigned int finish_read_trys = 0;
static unsigned int get_reads = 0;
static unsigned int get_read_trys = 0;
#define tb_count_fw __sync_add_and_fetch(&finish_writes, 1)
#define tb_count_fw_try __sync_add_and_fetch(&finish_write_trys, 1)
#define tb_count_fr __sync_add_and_fetch(&finish_reads, 1)
#define tb_count_fr_try __sync_add_and_fetch(&finish_read_trys, 1)
#define tb_count_gr __sync_add_and_fetch(&get_reads, 1)
#define tb_count_gr_try __sync_add_and_fetch(&get_read_trys, 1)
static void  tb_lock_prof_print(void) {
	printf("tribuf stats:\n\tcat        ups        trys\n");
	printf("\twrts: %9i %9i\n", finish_writes, finish_write_trys);
	printf("\tfrs:  %9i %9i\n", finish_reads, finish_read_trys);
	printf("\tgrs:  %9i %9i\n", get_reads, get_read_trys);
}
static void __attribute__((constructor)) tb_lock_prof_init(void) {
	atexit(tb_lock_prof_print);
}
#else
#define tb_count_fw
#define tb_count_fw_try
#define tb_count_fr
#define tb_count_fr_try
#define tb_count_gr
#define tb_count_gr_try
#endif

// only write thread will ever change aw, and only read thread will ever change ar
// when either thread wants a to put a new buffer into it's 'active' slot
// it can choose either nw or nr, (whichever is NOT -1) however the read thread
// will prefer nr and the write thread will prefer nw

// when the read thread is done it will move ar to nw and set ar to -1

// one of our invariants is that ar is -1 except between a matched pair of get_write and finish_write
// so get_write can just swap nr and ar to get the next read frame (since nr cannot be -1 when ar is)
// finish read needs to something different depending on where the -1 ended up since get_write (ar always gets -1 here)

typedef union {
	uint32_t v;
	struct { // invariant exactly one of these is always -1, likewise for 0, 1, 2
		int8_t aw; // never allowed to be -1 contains the number of the buffer we are currently writing to
		int8_t nw; // these two are 'available'
		int8_t nr;
		int8_t ar; // contains buffer we are reading from on other side
	};
}frms;

struct tribuf_s {
	void **data;
	frms active;
	uint32_t frame;
};

tribuf* tribuf_new(void **data, int locking)
{ (void)locking;
	tribuf *tb = xmalloc(sizeof(tribuf));

	tb->data = data;
	tb->frame = 0;
	tb->active.aw =  0;
	tb->active.nw =  1;
	tb->active.nr =  2;
	tb->active.ar = -1;

	return tb;
}

void tribuf_destroy(tribuf *tb)
{
	free(tb);
}

void* tribuf_get_write(tribuf *tb)
{
	frms active;
	active.v = tb->active.v;

	tb_sanity_check(active.aw < 0, "tribuf state inconsistent!");
	return tb->data[active.aw];
}

void tribuf_finish_write(tribuf *tb)
{
	frms f, active;
	tb_count_fw;
	do {
		active.v = tb->active.v;
		tb_count_fw_try;

		tb_sanity_check(active.aw < 0, "tribuf state inconsistent!");
		tb_sanity_check(active.nw < 0 && (active.nr < 0 || active.ar < 0), "tribuf state inconsistent!");

		f.nr = active.aw;
		f.ar = active.ar;

		if(active.nw < 0) { // only steal nr if we absolutly have to (ie we're running faster than read side can draw)
			f.aw = active.nr;
			f.nw = active.nw;
		} else {
			f.nw = active.nr;
			f.aw = active.nw;
		}
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));

	__sync_add_and_fetch(&tb->frame, 1);
	tb_sanity_check(f.aw < 0, "tribuf state inconsistent!");
}

void tribuf_finish_read(tribuf *tb)
{
	frms f, active;
	tb_count_fr;
	do {
		active.v = tb->active.v;
		tb_count_fr_try;

		tb_sanity_check(active.ar < 0, "tribuf_finish_read without tribuf_get_read!");
		tb_sanity_check(active.nw < 0 && active.nr < 0, "tribuf state inconsistent!");
		tb_sanity_check(active.nw >= 0 && active.nr >= 0, "tribuf state inconsistent!");

		f.aw = active.aw;
		f.ar = -1;
		if(active.nw < 0) {
			f.nw = active.ar;
			f.nr = active.nr;
		} else {
			f.nw = active.nw;
			f.nr = active.ar;
		}
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));

	tb_sanity_check(f.ar >= 0, "tribuf state inconsistent!");
}

void* tribuf_get_read(tribuf *tb)
{
	frms f, active;
	tb_count_gr;
	do {
		active.v = tb->active.v;
		tb_count_gr_try;

		tb_sanity_check(active.ar >= 0, "tribuf_get_read without finish_read!\n");
		tb_sanity_check(active.nw < 0 || active.nr < 0 || active.aw < 0, "tribuf state inconsistent!");

		f.aw = active.aw;
		f.nw = active.nw;
		f.ar = active.nr; // just swap nr and ar
		f.nr = active.ar;
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));

	tb_sanity_check(f.ar < 0, "tribuf state inconsistent!");
	return tb->data[f.ar];
}

void* tribuf_get_read_nolock(tribuf *tb) { // should only be used by write side to start up
	frms active;
	active.v = tb->active.v;
	int r = (active.nr<0)?active.ar:active.nr;
	return tb->data[r];
}

int tribuf_get_frmnum(tribuf *tb) {
	return tb->frame;
}
