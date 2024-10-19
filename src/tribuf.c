
#include "tribuf.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

//#define TB_DEBUG 1
//#define TB_DEBUG_USER 1
//#define TB_NO_ATOMIC 1

// defining TB_DEBUG_USER to 1 will cause the implementation to take locks in
// such a way that many incorrect uses of the tribuf will cause a deadlock
// so that you can debug your use of the tribuf with any of the various lock
// debugging tools.

#if __GNU_LIBRARY__
#	include <execinfo.h>
	static inline void tb_backtrace_stderr(void)
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
#	pragma message "Using C11 atomics"
#elif defined(__GNUC__) && !defined(TB_NO_ATOMIC)
#	if 0 //__GNUC__ >= 4 && __GNUC_MINOR__ >= 7
		// use the gcc built in equivelents of stdatomic funcs since we have them
		typedef int memory_order;
		typedef uint_fast16_t atomic_uint_fast16_t;
		typedef uint_fast32_t atomic_uint_fast32_t;
#		define memory_order_relaxed __ATOMIC_RELAXED
#		define memory_order_consume __ATOMIC_CONSUME
#		define memory_order_acquire __ATOMIC_ACQUIRE
#		define memory_order_release __ATOMIC_RELEASE
#		define memory_order_acq_rel __ATOMIC_ACQ_REL
#		define memory_order_seq_cst __ATOMIC_SEQ_CST
#		define atomic_init(obj, v) __atomic_store_n(obj, v, __ATOMIC_SEQ_CST)
#		define atomic_load(v) __atomic_load_n(v, __ATOMIC_SEQ_CST)
#		define atomic_load_explicit(v, o) __atomic_load_n(v, o)
#		define atomic_compare_exchange_weak_explicit(a, e, n, order, order2) __atomic_compare_exchange_n((a), (e), (n), 1, (order), (order2))
#		pragma message "Using gcc __atomic_* builtins for atomic ops"
#	else
		typedef enum {
			memory_order_relaxed,
			memory_order_consume,
			memory_order_acquire,
			memory_order_release,
			memory_order_acq_rel,
			memory_order_seq_cst
		} memory_order;

		typedef volatile uint_fast16_t atomic_uint_fast16_t;
		typedef volatile uint_fast32_t atomic_uint_fast32_t;

		//TODO: something that works off x86
#		define atomic_init(obj, v) do { *(obj) = (v); } while(0)
#		define atomic_load(v) (*(v))
#		define atomic_load_explicit(v, o) (*(v))
#		define atomic_compare_exchange_weak_explicit(a, e, n, order, order2) __sync_bool_compare_and_swap(a, *(e), n)
#		pragma message "Using gcc __sync_* builtins for atomic ops"
#	endif
#else
#	define atomic_init(obj, v) do { *(obj) = (v); } while(0)
#	ifndef TB_NO_ATOMIC
#		warning "NO ATOMICS, using threads and locks"
#		define TB_NO_ATOMIC 1
#	endif
#endif


// if we have c11 and gcc is new enough that it's not lying about thread support
// use real C11 threads
#if defined(TB_DEBUG_USER) || defined(TB_NO_ATOMIC)
# if (__STDC_VERSION__ >= 201102L) && !defined(__STDC_NO_THREADS__) && (defined(__clang__) || !(defined(__GNUC__) && defined(__GNUC_MINOR__) && (((__GNUC__ << 8) | __GNUC_MINOR__) >= ((4 << 8) | 9))))
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
			//error(0, r, "TRIBUF LOCK FAILED!");
			abort();
		}
	}
	static inline int tb_trylock(tb_mutex *m) {
		int r = pthread_mutex_trylock(m);
		if(r && r != EBUSY) {
			//error(0, r, "TRIBUF TRYLOCK FAILED!");
			abort();
		}
		return !r;
	}
	static inline void tb_unlock(tb_mutex *m) {
		int r = pthread_mutex_unlock(m);
		if(r) {
			//error(0, r, "TRIBUF UNLOCK FAILED!");
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
		frms t; t.v = (state).v; \
		if(t.ar == t.aw || t.ar == t.id || t.aw == t.id) \
			tribuf_error("tribuf duplicate buffer!\n"); \
	} while(0)
// note aw is special, it should always have an actual buffer in it (not -1)
#else
#define tb_sanity_check(c, m) do { } while(0)
#define tb_check_invariants(f) do { } while(0)
#endif

// only write thread will ever change aw, and only read thread will ever change ar
// when either thread wants a to put a new buffer into it's 'active' slot
// it takes the idle buffer

typedef union frms {
	uint_fast16_t v;
	struct {
		unsigned int idf: 1; ///< 1 if the idle buffer is newer than the read buffer
		unsigned int aw : 2; ///< current write buffer
		unsigned int id : 2; ///< current idle buffer
		unsigned int ar : 2; ///< current read buffer
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
#ifndef TB_NO_ATOMIC
	//atomic_uint_fast32_t frame; //< holds a count of the number of frames finished
	atomic_uint_fast16_t active; //< hold the current state of buffer assignment
#else
	uint_fast16_t active; //< hold the current state of buffer assignment
#endif
};

//***************************** init and teardown ******************************

tribuf* tribuf_new(void **data, int locking)
{ (void)locking;
	tribuf *tb = malloc(sizeof(tribuf));
	if(tb == NULL) abort();

	tb->data = data;
	//atomic_init(&tb->frame, 0);
	frms tmp;
	tmp.v = 0;
	tmp.idf = 0;
	tmp.aw =  0;
	tmp.id =  1;
	tmp.ar =  2;
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

static frms do_finish_write(frms active)
{
	frms f;
	f.v = 0;
	f.idf = 1;
	f.aw = active.id;
	f.id = active.aw;
	f.ar = active.ar;

	tb_check_invariants(active);
	tb_check_invariants(f);
	return f;
}

static frms do_get_read(frms active)
{
	frms f;
	f.v = 0;
	f.idf = 0;
	f.aw = active.aw;

	// check the fresh bit to see if we've got a new buffer,
	// if not just return the same one as last time because it's still the
	// most recent one
	if(active.idf) {
		f.id = active.ar;
		f.ar = active.id;
	} else {
		f.id = active.id;
		f.ar = active.ar;
	}

	tb_check_invariants(active);
	tb_check_invariants(f);
	return f;
}

static int do_get_read_nolock(frms active) {
	return active.ar;
}


// ********************* syncronization wrappers for logic *********************

// here we have the functions that are actually visible, they handle syncronization
// there are two different implementations, one using locks and one using atomics

//TODO: double check ordering stuff
#ifndef TB_NO_ATOMIC

void* tribuf_get_write(tribuf *tb)
{
	frms active;
	active.v = atomic_load_explicit(&tb->active, memory_order_consume);

	user_debug_lock(tb, write);

	return tb->data[active.aw];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	frms f, active;

	active.v = atomic_load_explicit(&tb->active, memory_order_consume);
	do {
		f = do_finish_write(active);
	} while(!atomic_compare_exchange_weak_explicit(&tb->active, &active.v, f.v, memory_order_release, memory_order_relaxed));
}

void* tribuf_get_read(tribuf *tb)
{
	frms f, active;
	active.v = atomic_load_explicit(&tb->active, memory_order_consume);
	do {
		f = do_get_read(active);
	} while(!atomic_compare_exchange_weak_explicit(&tb->active, &active.v, f.v, memory_order_acquire, memory_order_relaxed));

	user_debug_lock(tb, read);

	return tb->data[f.ar];
}

void tribuf_finish_read(tribuf *tb) // NOOP with debugging off
{(void)tb;
	user_debug_unlock(tb, read);
}

void* tribuf_get_read_nolock(tribuf *tb) { // should only be used by write side to start up
	frms active;
	active.v = atomic_load_explicit(&tb->active, memory_order_acquire);
	return tb->data[do_get_read_nolock(active)];
}

int tribuf_check_fresh(tribuf *tb)
{
	frms active;
	active.v = atomic_load_explicit(&tb->active, memory_order_consume); //TODO: can this be memory_order_relaxed?
	return active.idf;
}

#else // TB_NO_ATOMIC defined
/************************************* LOCK BASED SYNC *************************/

void* tribuf_get_write(tribuf *tb)
{
	tb_lock(&tb->lock);
	int r = ((frms)tb->active).aw;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, write);
	return tb->data[r];
}

void tribuf_finish_write(tribuf *tb)
{
	user_debug_unlock(tb, write);
	tb_lock(&tb->lock);
	tb->active = do_finish_write((frms)tb->active).v;
	tb_unlock(&tb->lock);
}

void* tribuf_get_read(tribuf *tb)
{
	tb_lock(&tb->lock);
	frms f = do_get_read((frms)tb->active);
	tb->active = f.v;
	tb_unlock(&tb->lock);
	user_debug_lock(tb, read);
	return tb->data[f.ar];
}

void tribuf_finish_read(tribuf *tb) // NOOP with debugging off
{
	user_debug_unlock(tb, read);
}

void* tribuf_get_read_nolock(tribuf *tb)
{
	tb_lock(&tb->lock);
	int r = do_get_read_nolock((frms)tb->active);
	tb_unlock(&tb->lock);
	return tb->data[r];
}

int tribuf_check_fresh(tribuf *tb)
{
	tb_lock(&tb->lock);
	int res = ((frms)tb->active).idf;
	tb_unlock(&tb->lock);
	return res;
}
#endif

//TODO: add tests to make sure we never get stale stuff out of this
//TODO: add a stress test
