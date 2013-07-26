
#include "tribuf.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#define TB_DEBUG 1
//#define TB_DEBUG_USER 1
//#define TRIBBUF_PROFILE 1
//#define TB_NO_ATOMIC 1

// defining TB_DEBUG_USER to 1 will cause the implementation to take locks in
// such a way that many incorrect uses of the tribuf will cause a deadlock
// so that you can debug your use of the tribuf with any of the various lock 
// debugging tools.

#if __GNU_LIBRARY__
#	include <execinfo.h>
	static void tb_backtrace_stderr(void)
	{
		void *bt_buf[20];
		fprintf(stderr, "Backtrace:\n");
		fflush(stderr);
		size_t size = backtrace(bt_buf, 20);
		backtrace_symbols_fd(bt_buf, size, STDERR_FILENO);
	}
#else
#	define tb_backtrace_stderr() do { } while(0)
#endif

#define tb_abort_if(c)  do {\
	if( (c) ) { \
	fprintf(stderr, "Abort in function '%s':\n%s:%d", \
			__func__, __FILE__, __LINE__); \
	tb_backtrace_stderr(); \
	abort(); } } while(0)
#define tribuf_error(s) do {\
		fflush(stdout); \
		fprintf(stderr, "%s: In function '%s':\n", __FILE__, __func__); \
		fprintf(stderr, "%s:%i error: %s", __FILE__, __LINE__, (s)); \
		fflush(stderr); \
		tb_backtrace_stderr(); \
		abort();\
	} while(0)

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__) && !defined(TB_NO_ATOMIC)
#	define TB_STDATOMIC 1
#	include <stdatomic.h>
// use relaxed order for when the exchange fails
#	define tb_comp_xchng(a, e, n, order) \
		atomic_compare_exchange_weak_explicit(a, e, n, \
			       order, \
			       memory_order_relaxed)
#	define tb_atomic_inc(x) do { atomic_fetch_add_explicit(a, 1, memory_order_relaxed); } while(0)
#elif defined(__GNUC__)
	typedef enum {
		memory_order_relaxed,
		memory_order_consume,
		memory_order_acquire,
		memory_order_release,
		memory_order_acq_rel,
		memory_order_seq_cst
	} memory_order;
	typedef uint_fast16_t atomic_uint_fast16_t;
	typedef uint_fast32_t atomic_uint_fast32_t;

	//TODO: something that works off x86
#	define ATOMIC_VAR_INIT(v) (v)
#	define atomic_init(obj, v) do { *obj = v; } while(0)
#	define atomic_load(v) (*(v))
#	define atomic_load_explicit(v, o) (*(v))
	//#	define tb_comp_xchng(a, e, n, o) __sync_bool_compare_and_swap(a, *e, n)
	static inline bool tb_comp_xchng(uint_fast16_t *a, uint_fast16_t *e, uint_fast16_t n, memory_order o)
	{
		(void)o;
	//	return __sync_bool_compare_and_swap(a, *e, n);
		uint_fast16_t t = __sync_val_compare_and_swap(a, *e, n);
		if(t != *e) {
			*e = t;
			return 0;
		}
		return 1;
	}
#	define tb_atomic_inc(x) do { __sync_add_and_fetch(x, 1); } while(0)
#	warning "Using gcc __sync_* builtins for atomic ops"
#else
#	warning "NO ATOMICS, using threads and locks"
#	define TB_NO_ATOMIC 1
#endif

#if defined(TB_DEBUG_USER) || defined(TB_NO_ATOMIC)
# if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#	include <threads.h>	
	typedef mtx_t tb_mutex;
	static inline void tb_lock(tb_mutex *m) {
		int r = mtx_lock(m);
		tb_abort_if(r != thrd_success);
	}
	static inline bool tb_trylock(tb_mutex *m) {
		int r = mtx_trylock(m);
		tb_abort_if(r == thrd_error);
		return r == thrd_success;
	}
	static inline void tb_unlock(tb_mutex *m) {
		int r = mtx_unlock(m);
		tb_abort_if(r != thrd_success);
	}
	static inline void tb_mutex_init(tb_mutex *m) {
		int r = mtx_init(m, mtx_plain);
		tb_abort_if(r != thrd_success);
	}
	static inline void tb_mutex_destroy(tb_mutex *m) {
		int r = mtx_destroy(m, mtx_destroy);
		tb_abort_if(r != thrd_success);
	}
# else // no C11 threads, use pthreads
#	include <sys/types.h>
#	include <pthread.h>
#	include <error.h>
#	include <errno.h>
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
#	define tb_mutex_destroy pthread_mutex_destroy
#	define tb_mutex_init(m) pthread_mutex_init((m), NULL)
# endif // __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#endif // defined(TB_DEBUG_USER) || defined(TB_NO_ATOMIC)

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

#ifdef TB_DEBUG
// invariant checking macro
#define tb_sanity_check(c, m) do {\
		if((c)) tribuf_error((m));\
	} while(0)
#define tb_check_invariants(state) do {\
		frms t; t.v = (state).v;\
		if((int)(bool)t.arf + (int)(bool)t.nwf + (int)(bool)t.nrf != 1) tribuf_error("tribuf bad fresh bits!\n");\
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

#if defined(TRIBBUF_PROFILE) && (defined(TB_STDATOMIC) || defined(__GNUC__))
static atomic_uint_fast32_t finish_writes = ATOMIC_VAR_INIT(0);
static atomic_uint_fast32_t finish_write_trys = ATOMIC_VAR_INIT(0);
static atomic_uint_fast32_t finish_reads = ATOMIC_VAR_INIT(0);
static atomic_uint_fast32_t finish_read_trys = ATOMIC_VAR_INIT(0);
static atomic_uint_fast32_t get_reads = ATOMIC_VAR_INIT(0);
static atomic_uint_fast32_t get_read_trys = ATOMIC_VAR_INIT(0);
#define tb_count_fw tb_atomic_inc(&finish_writes)
#define tb_count_fw_try tb_atomic_inc(&finish_write_trys)
#define tb_count_fr tb_atomic_inc(&finish_reads)
#define tb_count_fr_try tb_atomic_inc(&finish_read_trys)
#define tb_count_gr tb_atomic_inc(&get_reads)
#define tb_count_gr_try tb_atomic_inc(&get_read_trys)
static void  tb_lock_prof_print(void) {
	printf("tribuf stats:\n\tcat        ups        trys\n"); //TODO: use correct format specifier
	printf("\twrts: %9i %9i\n", (int)atomic_load(&finish_writes), (int)atomic_load(&finish_write_trys));
	printf("\tfrs:  %9i %9i\n", (int)atomic_load(&finish_reads), (int)atomic_load(&finish_read_trys));
	printf("\tgrs:  %9i %9i\n", (int)atomic_load(&get_reads), (int)atomic_load(&get_read_trys));
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
// it can choose either nw or nr, (whichever is NOT -1)

// the choice between nr and nw when both are not -1 is based on the 'fresh' bits
// there is a bit for each of ar, nw, and nr that indicates it is 'fresh'
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

// The names of the 'available' buffer fields in this struct are stupid now, but 
// they used to mean something and I can't think of better names for now
// so they keep the ones they have (They used to mean 'next write' and 'next read'
// and when both were non-negative we picked a specific one in an attempt to always
// get the 'freshest' data... it didn't work)

typedef union frms {
	uint_fast16_t v;
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
#ifdef TB_DEBUG_USER
	tb_mutex debug_read_lock, debug_write_lock;
#endif

// might want to do something to force this onto a seperate cache line from 
// everything else so that the lock elision coming in glibc doesn't always abort
// since the only thing we do that might cause an abort is write to the ints if
// they share a cache line
#ifdef TB_NO_ATOMIC
	tb_mutex lock;
#endif

	void **data;

	atomic_uint_fast32_t frame; //< holds a count of the number of frames finished
	atomic_uint_fast16_t active; //< hold the current state of buffer assignment
};

//***************************** init and teardown ******************************

tribuf* tribuf_new(void **data, int locking)
{ (void)locking;
	tribuf *tb = malloc(sizeof(tribuf));
	if(tb == NULL) abort();

	tb->data = data;
	atomic_init(&tb->frame, 0);
	frms tmp;
	tmp.v = 0;
	tmp.nrf = 1;
	tmp.aw =  0;
	tmp.nw =  1;
	tmp.nr =  2;
	tmp.ar = -1;
	atomic_init(&tb->active, tmp.v);

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

// *********************** logic and management *****************************

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

	if(active.nw < 0) {
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

static int do_get_read_nolock(frms active) {
	//tb_check_invariants(active);
	//return (active.nr<0)?active.ar:active.nr;
	if(active.nrf) return active.nr;
	else if(active.arf) return active.ar;
	else return active.nw;
}


// ********************* syncronization wrappers for logic *********************

// here we have the functions that are actually visible, they handle syncronization
// there are two different implementations, one using locks and one using atomics

#ifndef TB_NO_ATOMIC

void* tribuf_get_write(tribuf *tb)
{
	frms f, active;
	active.v = atomic_load_explicit(&tb->active, memory_order_relaxed);
	
	f = do_get_write(active); (void)f;
	// If we were looping here I think it could use memory_order_relaxed on the xcng
	
	user_debug_lock(tb, write);
	
	return tb->data[active.aw];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	frms f, active;
	tb_count_fw;
	
	active.v = atomic_load_explicit(&tb->active, memory_order_relaxed);
	do {
		tb_count_fw_try;
		f = do_finish_write(active);
	} while(!tb_comp_xchng(&tb->active, &active.v, f.v, memory_order_release));
	//TODO: double check ordering stuff
	
	//TODO: work out weather or not we really need this to be atomic
	tb_atomic_inc(&tb->frame);
	// if we make the inc of tb->frame non-atomic it needs to go before 
	// the loop so it gets included in the release fence for the successful CAS
}

void* tribuf_get_read(tribuf *tb)
{
	frms f, active;
	tb_count_gr;
	
	active.v = atomic_load_explicit(&tb->active, memory_order_relaxed);
	do {
		tb_count_gr_try;
		f = do_get_read(active);
	} while(!tb_comp_xchng(&tb->active, &active.v, f.v, memory_order_acquire));
	//TODO: double check ordering stuff, can we use consume here?

	user_debug_lock(tb, read);

	return tb->data[f.ar];
}

void tribuf_finish_read(tribuf *tb)
{
	user_debug_unlock(tb, read);

	frms f, active;
	tb_count_fr;
	
	active.v = atomic_load_explicit(&tb->active, memory_order_relaxed);
	do {
		tb_count_fr_try;
		f = do_finish_read(active);
	} while(!tb_comp_xchng(&tb->active, &active.v, f.v, memory_order_relaxed));
	//TODO: double check ordering stuff
}

void* tribuf_get_read_nolock(tribuf *tb) { // should only be used by write side to start up
	frms active;
	active.v = atomic_load_explicit(&tb->active, memory_order_relaxed);
	return tb->data[do_get_read_nolock(active)];
}

int tribuf_get_frmnum(tribuf *tb) {
	//TODO: do we really want relaxed here?
	//return atomic_load_explicit(&tb->frame, memory_order_consume);
	return atomic_load_explicit(&tb->frame, memory_order_relaxed);
}

#else // TB_NO_ATOMIC defined

void* tribuf_get_write(tribuf *tb)
{
	tb_lock(&tb->lock);
	frms f = do_get_write((frms)tb->active);
	tb->active = f.v;
	int r = f.aw;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, write);
	return tb->data[r];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	tb_lock(&tb->lock);
	tb->active = do_finish_write((frms)tb->active).v;
	tb->frame++;
	tb_unlock(&tb->lock);
}

void* tribuf_get_read(tribuf *tb)
{
	tb_lock(&tb->lock);
	frms f = do_get_read((frms)tb->active);
	int r = f.ar;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, read);
	return tb->data[r];
}

void tribuf_finish_read(tribuf *tb)
{
	user_debug_unlock(tb, read);
	tb_lock(&tb->lock);
	tb->active = do_finish_read((frms)tb->active).v;
	tb_unlock(&tb->lock);
}

void* tribuf_get_read_nolock(tribuf *tb)
{
	tb_lock(&tb->lock);
	int r = do_get_read_nolock((frms)tb->active);
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
