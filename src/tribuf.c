
#include "common.h"
#include "tribuf.h"

//#ifndef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
// if we can't CAS 4 bytes then nothing works!
//#define TB_NO_ATOMIC
//#endif

//#define TB_DEBUG 1
//#define TB_DEBUG_USER 1
//#define TRIBBUF_PROFILE 1

#if defined(TB_DEBUG_USER) || defined(TB_NO_ATOMIC)
# include <sys/types.h>
# include <pthread.h>
# include <error.h>
# include <errno.h>
# include <assert.h>

typedef pthread_mutex_t tb_mutex;
static inline void tb_lock(tb_mutex *m) {
	int r = pthread_mutex_lock(m);
	if(r) {
		error(0, r, "TRIBUF LOCK FAILED!");
		abort();
	}
}

static inline int tb_trylock(tb_mutex *m) {
	int r = pthread_mutex_trylock(m);
	if(r && r != EBUSY) {
		error(0, r, "TRIBUF TRYLOCK FAILED!");
		abort();
	}
	return !r;
}

static inline void tb_unlock(tb_mutex *m) {
	int r = pthread_mutex_unlock(m);
	if(r) {
		error(0, r, "TRIBUF UNLOCK FAILED!");
		abort();
	}
}

#define tb_mutex_destroy pthread_mutex_destroy
#define tb_mutex_init(m) pthread_mutex_init((m), NULL)
#endif

#ifdef TB_DEBUG_USER
#define user_debug_lock(tb, action) do { \
	if(!tb_trylock(&(tb)->debug_##action##_lock)) abort();\
	} while(0)
#define user_debug_unlock(tb, action) do { \
	tb_unlock(&(tb)->debug_##action##_lock);\
	} while(0)
#else
#define user_debug_lock(tb, action) do { } while(0)
#define user_debug_unlock(tb, action) do { } while(0)
#endif

#define tribuf_error(s) do {\
		fflush(stdout); \
		fprintf(stderr, "%s: In function '%s':\n", __FILE__, __func__); \
		fprintf(stderr, "%s:%i error: %s", __FILE__, __LINE__, (s)); \
		fflush(stderr); \
		abort();\
	} while(0)

#ifdef TB_DEBUG
#define tb_sanity_check(c, m) do {\
		if((c)) tribuf_error((m));\
	} while(0)

#define tb_check_invariants(state) do {\
	frms t; t.v = (state).v;\
	if((unsigned)t.arf + (unsigned)t.nwf + (unsigned)t.nrf == 1) tribuf_error("tribuf bad fresh bits!\n");\
	if(   t.ar == t.aw || t.ar == t.nr || t.ar == t.nw \
	   || t.aw == t.nr || t.aw == t.nw \
	   || t.nr == t.nw) \
		tribuf_error("tribuf duplicate buffer!\n");\
	if(t.arf && t.ar < 0) tribuf_error("bad fresh bit!\n"); \
	if(t.nrf && t.nr < 0) tribuf_error("bad fresh bit!\n"); \
	if(t.nwf && t.nw < 0) tribuf_error("bad fresh bit!\n"); \
	if(t.ar > 2 || t.ar < -1 ) tribuf_error("bad buffer index (ar)!\n"); \
	if(t.nr > 2 || t.nr < -1 ) tribuf_error("bad buffer index (nr)!\n"); \
	if(t.nw > 2 || t.nw < -1 ) tribuf_error("bad buffer index (nw)!\n"); \
	if(t.aw > 2 || t.aw <  0 ) tribuf_error("bad buffer index (aw)!\n"); \
	} while(0)
// note aw is special, it should always have an actual buffer in it (not -1)
#else
#define tb_sanity_check(c, m) do { } while(0)
#define tb_check_invariants(f) do { } while(0)
#endif

#if defined(TRIBBUF_PROFILE) && defined(__GNUC__)
static uint32_t finish_writes = 0;
static uint32_t finish_write_trys = 0;
static uint32_t finish_reads = 0;
static uint32_t finish_read_trys = 0;
static uint32_t get_reads = 0;
static uint32_t get_read_trys = 0;
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
#define tb_count_fw do { } while(0)
#define tb_count_fw_try do { } while(0)
#define tb_count_fr do { } while(0)
#define tb_count_fr_try do { } while(0)
#define tb_count_gr do { } while(0)
#define tb_count_gr_try do { } while(0)
#endif

// only write thread will ever change aw, and only read thread will ever change ar
// when either thread wants a to put a new buffer into it's 'active' slot
// it can choose either nw or nr, (whichever is NOT -1) however the read thread
// will prefer nr and the write thread will prefer nw

// the choice between nr and nw when each are not -1 is not quite just preference
// anymore, now we have a bit for each of ar, nw, and nr that indicates it is 'fresh'
// only the buffer most recently written to (post finish_write()) has it's fresh
// bit set, so on the write it side it really doesn't matter which we pick since
// we are about to set the fresh bit anyway, while on the read side we always take
// whichever one has it's fresh bit set. When moving a buffer to/from ar we move
// the fresh bit with it, this way if have a read side running faster than the
// write side the read side will still always get the freshest data, that is if
// we have sequence numbers in the tb and the read side does:
// b1 = get_read()
// finish_read()
// b2 = get_read()
// then b1 <= b2 always (provided the sequence numbers don't overflow)

typedef union {
	uint32_t v;
	struct {
		// invariant exactly one of these is 1, the other two are 0
		int arf : 1;
		int nwf : 1;
		int nrf : 1;
		
		// invariant exactly one of these is always -1, likewise for 0, 1, 2
		int aw : 3; // never allowed to be -1 contains the number of the buffer we are currently writing to
		int nw : 3; // these two are 'available'
		int nr : 3;
		int ar : 3; // contains buffer we are reading from on other side
	};
}frms;

struct tribuf_s {
	void **data;

#ifdef TB_DEBUG_USER
	tb_mutex debug_read_lock, debug_write_lock;
#endif

#ifdef TB_NO_ATOMIC
	tb_mutex lock;
#endif

	frms active;
	uint32_t frame;
};

tribuf* tribuf_new(void **data, int locking)
{ (void)locking;
	tribuf *tb = xmalloc(sizeof(tribuf));

	tb->data = data;
	tb->frame = 0;
	tb->active.v = 0;
	tb->active.nrf = 1;
	tb->active.aw =  0;
	tb->active.nw =  1;
	tb->active.nr =  2;
	tb->active.ar = -1;
	
#ifdef TB_DEBUG_USER
	tb_mutex_init(&tb->debug_read_lock);
	tb_mutex_init(&tb->debug_write_lock);
#endif

#ifdef TB_NO_ATOMIC
	tb_mutex_init(&tb->lock);
#endif

	return tb;
}

void tribuf_destroy(tribuf *tb)
{
#ifdef TB_DEBUG_USER
	tb_mutex_destroy(&tb->debug_read_lock);
	tb_mutex_destroy(&tb->debug_write_lock);
#endif

#ifdef TB_NO_ATOMIC
	tb_mutex_destroy(&tb->lock);
#endif

	free(tb);
}

static frms do_get_write(frms active)
{
	tb_sanity_check(active.aw < 0, "tribuf state inconsistent!");
	tb_check_invariants(active);
	return active;
}

static frms do_finish_write(frms active)
{
	tb_sanity_check(active.aw < 0, "tribuf state inconsistent!");
	tb_sanity_check(active.nw < 0 && (active.nr < 0 || active.ar < 0), "tribuf state inconsistent!");
	tb_check_invariants(active);
	
	frms f;
	f.v = 0;	
	f.nrf = 1;
	f.nr = active.aw;
	f.ar = active.ar;

	if(active.nw < 0) { // only steal nr if we absolutly have to (ie we're running faster than read side can draw)
		f.aw = active.nr;
		f.nw = active.nw;
	} else {
		f.nw = active.nr;
		f.aw = active.nw;
	}
	tb_sanity_check(f.aw < 0, "tribuf state inconsistent!");
	tb_check_invariants(f);
	return f;
}

static frms do_get_read(frms active)
{
	tb_sanity_check(active.ar >= 0, "tribuf_get_read without finish_read!\n");
	tb_sanity_check(active.nw < 0 || active.nr < 0 || active.aw < 0, "tribuf state inconsistent!");
	tb_check_invariants(active);
	
	frms f;
	f.v = 0;
	f.arf = 1;
	f.aw = active.aw;
	
	if(active.nrf) {
		f.nw = active.nw;
		f.ar = active.nr; // swap nr and ar
		f.nr = active.ar;
	} else {
		tb_sanity_check(active.nwf, "Bad fresh bits");
		f.ar = active.nw;
		f.nr = active.nr; // swap nw and ar
		f.nw = active.ar;
	}
	tb_sanity_check(f.ar < 0, "tribuf state inconsistent!");
	tb_check_invariants(f);
	return f;
}

static frms do_finish_read(frms active)
{
	tb_sanity_check(active.ar < 0, "tribuf_finish_read without tribuf_get_read!");
	tb_sanity_check(active.nw < 0 && active.nr < 0, "tribuf state inconsistent!");
	tb_sanity_check(active.nw >= 0 && active.nr >= 0, "tribuf state inconsistent!");
	tb_check_invariants(active);
	
	frms f;
	f.v = 0;
	f.aw = active.aw;
	f.ar = -1;
	if(active.nw < 0) {
		f.nwf = active.arf;
		f.nrf = active.nrf;
		f.nw = active.ar;
		f.nr = active.nr;
	} else {
		f.nwf = active.nwf;
		f.nrf = active.arf;
		f.nw = active.nw;
		f.nr = active.ar;
	}
	tb_sanity_check(f.ar >= 0, "tribuf state inconsistent!");
	tb_check_invariants(f);
	return f;
}

#ifndef TB_NO_ATOMIC

void* tribuf_get_write(tribuf *tb)
{
	frms f, active;
	active.v = tb->active.v;
	
	f = do_get_write(active); (void)f;
	
	user_debug_lock(tb, write);
	
	return tb->data[active.aw];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	frms f, active;
	tb_count_fw;
	do {
		active.v = tb->active.v;
		tb_count_fw_try;
		f = do_finish_write(active);
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));
	
	//TODO: work out weather or not we really need this to be atomic
	__sync_add_and_fetch(&tb->frame, 1);
}

void* tribuf_get_read(tribuf *tb)
{
	frms f, active;
	tb_count_gr;
	do {
		active.v = tb->active.v;
		tb_count_gr_try;
		f = do_get_read(active);
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));

	user_debug_lock(tb, read);

	return tb->data[f.ar];
}

void tribuf_finish_read(tribuf *tb)
{
	user_debug_unlock(tb, read);

	frms f, active;
	tb_count_fr;
	do {
		active.v = tb->active.v;
		tb_count_fr_try;
		f = do_finish_read(active);
	} while(!__sync_bool_compare_and_swap(&tb->active.v, active.v, f.v));
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

#else // TB_NO_ATOMIC defined

void* tribuf_get_write(tribuf *tb)
{
	tb_lock(&tb->lock);
	tb->active = do_get_write(tb->active);
	int r = tb->active.aw;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, write);
	return tb->data[r];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	tb_lock(&tb->lock);
	tb->active = do_finish_write(tb->active);
	tb->frame++;
	tb_unlock(&tb->lock);
}

void* tribuf_get_read(tribuf *tb)
{
	tb_lock(&tb->lock);
	tb->active = do_get_read(tb->active);
	int r = tb->active.ar;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, read);
	return tb->data[r];
}

void tribuf_finish_read(tribuf *tb)
{
	user_debug_unlock(tb, read);
	tb_lock(&tb->lock);
	tb->active = do_finish_read(tb->active);
	tb_unlock(&tb->lock);
}

void* tribuf_get_read_nolock(tribuf *tb)
{
	tb_lock(&tb->lock);
	int r = (tb->active.nr<0)?tb->active.ar:tb->active.nr;
	tb_unlock(&tb->lock);
	return tb->data[r];
}

int tribuf_get_frmnum(tribuf *tb)
{
	tb_lock(&tb->lock);
	int res = tb->frame;
	tb_unlock(&tb->lock);
	return res;
}
#endif

//TODO: add tests to make sure we never get stale stuff out of this
//TODO: add a stress test
