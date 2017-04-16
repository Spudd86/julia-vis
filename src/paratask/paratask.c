#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <malloc.h>
#include <assert.h>

#include "paratask.h"

// if we have c11 and gcc is new enough that it's not lying about thread support
// use real C11 threads
#if (__STDC_VERSION__ >= 201102L) && !defined(__STDC_NO_THREADS__) && (defined(__clang__) || !(defined(__GNUC__) && defined(__GNUC_MINOR__) && (((__GNUC__ << 8) | __GNUC_MINOR__) >= ((4 << 8) | 9))))
#	include <threads.h>
#else
#	include "tinycthread.h"
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__) && !defined(PARATASK_NO_ATOMICS)
#	include <stdatomic.h>
#elif defined(__GNUC__) && !defined(PARATASK_NO_ATOMICS)
#	if __GNUC__ >= 4 && __GNUC_MINOR__ >= 7
		// use the gcc built in equivelents of stdatomic funcs since we have them
		typedef int memory_order;
		typedef size_t atomic_size_t;
#		define memory_order_relaxed __ATOMIC_RELAXED
#		define memory_order_consume __ATOMIC_CONSUME
#		define memory_order_acquire __ATOMIC_ACQUIRE
#		define memory_order_release __ATOMIC_RELEASE
#		define memory_order_acq_rel __ATOMIC_ACQ_REL
#		define memory_order_seq_cst __ATOMIC_SEQ_CST
#		define atomic_init(obj, v) __atomic_store_n(obj, v, __ATOMIC_SEQ_CST)
#		define atomic_fetch_add_explicit(v, a, o) __atomic_fetch_add(v, a, o)
#		pragma message "Using GCC memory model aware atomics"
#	else
		typedef volatile size_t atomic_size_t;
		typedef enum {
			memory_order_relaxed,
			memory_order_consume,
			memory_order_acquire,
			memory_order_release,
			memory_order_acq_rel,
			memory_order_seq_cst
		} memory_order;
#		define atomic_fetch_add_explicit(obj, arg, order) __sync_fetch_and_add((obj), (arg))
#		define atomic_init(obj, init) do { *(obj) = (init); __sync_synchronize(); } while(0)
#		pragma message "Using GCC atomics (no memory model)"
#	endif
#else
#	define PARATASK_NO_ATOMICS 1
#	pragma message "no atomics"
#endif

struct paratask_task
{
	struct paratask_ctx *ctx;
	paratask_task_fn func;
	void *fn_arg;

	size_t work_offset;
	size_t work_size;

	int work_complete;
	int nthreads_working;
	mtx_t task_mtx;
	cnd_t idle_cnd;

	struct paratask_task *next_task;

#if PARATASK_NO_ATOMICS
	size_t next_work_id;
#else
	atomic_size_t next_work_id;
#endif

};

struct paratask_ctx
{
	int nthreads;       ///< number of threads in our pool
	bool set_affinity;

	mtx_t queue_mtx;
	cnd_t queue_cnd;    ///< used to signal worker threads to grab work, must hold queue_mtx

	cnd_t shutdown_cnd; ///< used for signalling that worker threads have finished exiting
	int shutting_down;  ///< if set worker threads should exit instead of grabbing work
	int nthreads_alive; ///< number of worker threads that are currently active, unless we are starting up or shutting down should be equal to nthreads

	struct paratask_task *next_task; ///< head of task queue
	struct paratask_task *last_task; ///< tail of task queue

	thrd_t threads[];   ///< all the threads in our pool
};

once_flag default_instance_once_flag = ONCE_FLAG_INIT;
static struct paratask_ctx *default_instance = NULL;
static void create_default_instance(void) { default_instance = paratask_new(0); }
struct paratask_ctx *paratask_default_instance(void) {
	call_once(&default_instance_once_flag, create_default_instance);
	return default_instance;
}

static int task_thread(void *);
static int default_thread_count(void);

struct paratask_ctx *paratask_new(int nthreads)
{
	bool set_affinity = false;
	if(nthreads == 0) {
		nthreads = default_thread_count();
		set_affinity = true;
	} else if(nthreads < 0) {
		nthreads = -nthreads;
		int dflt = default_thread_count();
		if(nthreads > dflt) {
			nthreads = dflt;
			set_affinity = true;
		}
	}

	struct paratask_ctx *self = malloc(sizeof(*self) + sizeof(self->threads[0])*nthreads);
	if(!self) return NULL;

	self->nthreads = nthreads;
	self->set_affinity = set_affinity;

	mtx_init(&self->queue_mtx, mtx_plain);
	cnd_init(&self->queue_cnd);

	cnd_init(&self->shutdown_cnd);
	self->shutting_down = 0;
	self->nthreads_alive = 0;

	self->next_task = NULL;
	self->last_task = NULL;

	for(int i=0; i < self->nthreads; i++) {
		int err = thrd_create(&self->threads[i], task_thread, self);
		if(err != thrd_success) {
			abort(); //FIXME: do better than this
		}
	}

	return self;
}

void paratask_delete(struct paratask_ctx *self)
{
	mtx_lock(&self->queue_mtx);
	self->shutting_down = 1;
	cnd_broadcast(&self->queue_cnd);

	while(self->nthreads_alive)
		cnd_wait(&self->shutdown_cnd, &self->queue_mtx);
	mtx_unlock(&self->queue_mtx);

	for(int i=0; i<self->nthreads; i++) {
		thrd_join(self->threads[i], NULL);
	}

	// mark all outstanding tasks as not going to finish
	struct paratask_task *cur = self->next_task;
	while(cur) {
		mtx_lock(&cur->task_mtx);
		cur->work_complete = -1;
		cnd_broadcast(&cur->idle_cnd);
		struct paratask_task *next = cur->next_task;
		mtx_unlock(&cur->task_mtx);

		cur = next;
	}

	mtx_destroy(&self->queue_mtx);
	cnd_destroy(&self->queue_cnd);
	cnd_destroy(&self->shutdown_cnd);

	free(self);
}

int paratask_call(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	struct paratask_task *task = paratask_call_async(self, work_offset, work_size, fn, arg);
	if(!task) return -1;

	paratask_wait(task);

	return 0;
}

struct paratask_task *paratask_call_async(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	struct paratask_task *task = malloc(sizeof(*task));
	if(!task) return NULL;

	mtx_lock(&self->queue_mtx);
	//FIXME: return errors instead of calling abort()
	// not too worried about hitting these though because that's a truly unreasonable
	// amount of work, especially on 64 bit systems
	if(work_size >= SIZE_MAX - work_offset) abort();
	if(work_size >= SIZE_MAX - self->nthreads - 1) abort(); // avoid overflow in picking work item ids in task thread

	task->ctx = self;
	task->work_offset = work_offset;
	task->work_size = work_size;
	task->func = fn;
	task->fn_arg = arg;

	task->next_task = NULL;
	task->work_complete = 0;
	task->nthreads_working = 0;

	if(thrd_success != mtx_init(&task->task_mtx, mtx_plain)) abort();
	cnd_init(&task->idle_cnd);

#if PARATASK_NO_ATOMICS
	task->next_work_id = 0;
#else
	atomic_init(&task->next_work_id, 0);
#endif

	if(!self->next_task) {
		assert(self->last_task == NULL);
		self->next_task = self->last_task = task;
	} else {
		assert(self->last_task);
		self->last_task->next_task = task;
		self->last_task = task;
	}
	cnd_broadcast(&self->queue_cnd);
	mtx_unlock(&self->queue_mtx);

	return task;
}

int paratask_wait(struct paratask_task *task)
{
	int retval = 0;
	mtx_lock(&task->task_mtx);
	// need to check both because nthreads_working will ALWAYS be zero the first
	// time through because the workers haven't had a chance to start because we
	// hold queue_mtx, and work_complete gets set when ANY thread runs out stuff
	// to do so it could be set while there's still another worker that hasn't
	// finished it's last work item
	while(task->nthreads_working != 0 || task->work_complete == 0)
		cnd_wait(&task->idle_cnd, &task->task_mtx);
	if(task->work_complete != 1) { // work didn't actually finish
		retval = -1;
	}

#if 0
	task->ctx = NULL;
	task->work_offset = 0;
	task->work_size = 0;
	task->func = NULL;
	task->fn_arg = NULL;
#endif
	mtx_unlock(&task->task_mtx);
	
	mtx_destroy(&task->task_mtx);
	cnd_destroy(&task->idle_cnd);

	free(task);

	return retval;
}

static size_t next_work_item(struct paratask_task *task)
{
	size_t work_item_id;

#if PARATASK_NO_ATOMICS
	mtx_lock(&task->task_mtx);
	work_item_id = task->next_work_id++;
	mtx_unlock(&task->task_mtx);
#else
	work_item_id = atomic_fetch_add_explicit(&task->next_work_id, 1, memory_order_relaxed);
#endif
	return work_item_id;
}

static void set_thread_name(int thread_num);
static void paratsk_set_thread_affinity(int thread_num);

static int task_thread(void *ctx)
{
	struct paratask_ctx *self = ctx;
	mtx_lock(&self->queue_mtx);
	int thread_num = self->nthreads_alive++;

	set_thread_name(thread_num);

	if(self->set_affinity)
		paratsk_set_thread_affinity(thread_num);

	while(1) {
		while(!self->next_task && !self->shutting_down) {
			cnd_wait(&self->queue_cnd, &self->queue_mtx);
		}
		if(self->shutting_down) break;

		struct paratask_task *task = self->next_task;

		// if we're the last thread to grab the task or the task is already done
		// remove it from the list
		mtx_lock(&task->task_mtx);
		size_t global_work_offset = task->work_offset;
		size_t global_work_size = task->work_size;
		paratask_task_fn work_func = task->func;
		void *work_fn_arg = task->fn_arg;

		task->nthreads_working++;
		mtx_unlock(&task->task_mtx);
		mtx_unlock(&self->queue_mtx);

		while(1) {
			size_t work_item_id = next_work_item(task);

			if(work_item_id >= global_work_size) break;

			work_func(work_item_id + global_work_offset, work_fn_arg);
		}

		mtx_lock(&self->queue_mtx);
		mtx_lock(&task->task_mtx);
		if(!task->work_complete) { // first worker to get finish removes task from queue
			self->next_task = task->next_task;
			if(!self->next_task) self->last_task = NULL;
			task->next_task = NULL;
		}
		task->work_complete = 1;
		task->nthreads_working--;
		if(!task->nthreads_working) {
			if(thrd_success != cnd_signal(&task->idle_cnd)) abort(); // failure here will almost certainly stall the main thread
		}
		mtx_unlock(&task->task_mtx);
	}

	self->nthreads_alive--;
	if(!self->nthreads_alive) {
		if(thrd_success != cnd_signal(&self->shutdown_cnd)) abort(); // failure here will almost certainly stall the main thread
	}
	mtx_unlock(&self->queue_mtx);

	return 0;
}

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <sys/prctl.h>
#elif defined(__NetBSD__)
#include <sched.h>
#include <sys/param.h>
#endif

static void set_thread_name(int thread_num)
{
	char thread_name[16];
	sprintf(thread_name, "paratsk-%04d", thread_num);
#if defined(__linux__)
	prctl(PR_SET_NAME, thread_name);

//#elif defined(__APPLE__) && defined(__MACH__)
//	pthread_setname_np(thread_name);
#endif
}

static void paratsk_set_thread_affinity(int thread_num)
{
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
	DWORD_PTR process_mask;
	DWORD_PTR system_mask;
	if(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
		DWORD_PTR mask = process_mask;
		//FIXME: write windows support
	}
#elif defined(__linux__) //exists in glibc >= 2.3.4 and in musl, Linux specific syscalls
	cpu_set_t set; //NOTE: will fail on systems with a huge number of cores (>1024), but this probably won't scale to that anyway
	if(sched_getaffinity(0, sizeof(set), &set) == 0) {
		int count = 0;
		for(size_t i=0; i<CPU_SETSIZE; i++) {
			if(CPU_ISSET(i, &set)) {
				if(count == thread_num) {
					cpu_set_t newset;
					CPU_ZERO(&newset);
					CPU_SET(i, &newset);
					sched_setaffinity(0, sizeof(newset), &newset); // we can't really do anything if this fails so just ignore that
					return;
				}
				count++;
			}
		}
	}
//#elif defined(__NetBSD__)
	//TODO: NetBSD >= 5 support via sched_getaffinity_np
#endif
}

static int default_thread_count(void)
{
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
	DWORD_PTR process_mask;
	DWORD_PTR system_mask;
	if(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
		DWORD_PTR mask = process_mask;
		for(int count = 0; mask != 0; mask = mask >> 1) {
			if (mask & 1) count++;
		}
		return count;
	}
#elif defined(__linux__) //exists in glibc >= 2.3.4 and in musl, Linux specific syscalls
	cpu_set_t set; //NOTE: will fail on systems with a huge number of cores (>1024), but this probably won't scale to that anyway
	if(sched_getaffinity(0, sizeof(set), &set) == 0) {
#ifdef CPU_COUNT
		return CPU_COUNT(&set);
#else
		int count = 0;
		for(size_t i=0; i<CPU_SETSIZE; i++) {
			if(CPU_ISSET(i, &set)) count++;
		}
		return count;
#endif
	}
//#elif defined(__NetBSD__)
	//TODO: NetBSD >= 5 support via sched_getaffinity_np
#elif defined(_SC_NPROCESSORS_ONLN)
	int count = sysconf(_SC_NPROCESSORS_ONLN);
	return count;
#endif
   return 2; // if we can't get anything just guess
}

