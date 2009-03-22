
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

typedef union {
	uint32_t v;
	struct { // invariant exactly one of these is always -1, likewise for 0, 1, 2
		int8_t aw; // never allowed to be -1
		int8_t nw;
		int8_t nr;
		int8_t ar;
	};
}frms;

struct tribuf_s {
	void **data;
	frms active;
	uint32_t frame;
};

tribuf* tribuf_new(void **data, int locking)
{
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
		
		if(active.nw < 0) {
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

void* tribuf_get_read_nolock(tribuf *tb) {
	frms active; 	
	active.v = tb->active.v;
	//__sync_synchronize();// not sure we need this, (might need it to inhibit compiler optimizations)
	int r = (active.nr<0)?active.ar:active.nr;
	return tb->data[r]; 
}

int tribuf_get_frmnum(tribuf *tb) {
	return tb->frame; //__sync_add_and_fetch(&tb->frame, 0);
}
