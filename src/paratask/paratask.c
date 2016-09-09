#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <malloc.h>
#include <assert.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "paratask.h"

// if we have c11 and gcc is new enough that it's not lying about thread support
// use real C11 threads
#if (__STDC_VERSION__ >= 201102L) && !defined(__STDC_NO_THREADS__) && !(defined(__GNUC__) && defined(__GNUC_MINOR__) && (((__GNUC__ << 8) | __GNUC_MINOR__) >= ((4 << 8) | 9)))
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
		typedef volatile size_t atomic_size_t;
#		define memory_order_relaxed __ATOMIC_RELAXED
#		define memory_order_consume __ATOMIC_CONSUME
#		define memory_order_acquire __ATOMIC_ACQUIRE
#		define memory_order_release __ATOMIC_RELEASE
#		define memory_order_acq_rel __ATOMIC_ACQ_REL
#		define memory_order_seq_cst __ATOMIC_SEQ_CST
#		define atomic_init(obj, v) __atomic_store_n(obj, v, __ATOMIC_SEQ_CST)
#		define atomic_store(obj, v) __atomic_store(obj, v)
#		define atomic_load(v) __atomic_load_n(v, __ATOMIC_SEQ_CST)
#		define atomic_load_explicit(v, o) __atomic_load_n(v, o)
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
#		define atomic_init(obj, init) do { *(obj) = (init); } while(0)
#		define atomic_store(obj, v) do { *(obj) = (init); __sync_synchronize(); } while(0)
#		define atomic_load_explicit(obj, order) *(obj)
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
	cnd_t idle_cnd;

	struct paratask_task *next_task;

#if PARATASK_NO_ATOMICS
	mtx_t work_id_mtx;
	size_t next_work_id;
#else
	atomic_size_t next_work_id;
#endif

};

struct paratask_ctx
{
	int nthreads;       ///< number of threads in our pool

	mtx_t queue_mtx;
	cnd_t queue_cnd;

	cnd_t shutdown_cnd;
	int shutting_down;
	int nthreads_alive;
	
	struct paratask_task *next_task;
	struct paratask_task *last_task;

	thrd_t threads[];   ///< all the threads in our pool
};

/* TODO:
 * switch to a queue of tasks. - Partially done.
 *   - when a task is enqueued it will have a certain number of threads that it wants to use
 *   - last thread to start working on a task removes the task from the queue (can do this because we'll be holding queue_mtx)
 *   - need to move the condition variable that the main thread uses to wait for work completion into the the task struct (so that it's per-task)
 *
 * Fix possible overflow of atomic size_t when getting work item ids
 *   - can do by switching to signed type and counting down from work_size
 *   - or just split the job in half and wait on the second task
 *       - would need some way to clean up first job properly in async case
 *
 * Maybe add support for 3 dimensional work item ids
 * 
 * If we detect number of cpus and have that many work threads we should
 * have each thread try to bind itself to a specific CPU
 *
 * if only 1 thread is requested and we get paratask_call() just do work in calling
 * thread.
 */

once_flag default_instance_once_flag = ONCE_FLAG_INIT;
static struct paratask_ctx *default_instance = NULL;
static void create_default_instance(void) { default_instance = paratask_new(0); }
struct paratask_ctx *paratask_default_instance(void) {
	call_once(&default_instance_once_flag, create_default_instance);
	return default_instance;
}

static int task_thread(void *);

// must be called with queue_mtx held
static void launch_task(struct paratask_ctx *self, struct paratask_task *task, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg);
static void task_wait(struct paratask_task *task);
static void clear_task(struct paratask_task *task);

static int default_thread_count(void);

struct paratask_ctx *paratask_new(int nthreads)
{
	if(nthreads == 0) {
		nthreads = default_thread_count();
	} else if(nthreads < 0) {
		nthreads = -nthreads;
		int dflt = default_thread_count();
		if(nthreads > dflt) nthreads = dflt;
	}

	struct paratask_ctx *self = malloc(sizeof(*self) + sizeof(thrd_t)*nthreads);
	if(!self) return NULL;

	self->nthreads = nthreads;

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
	//TODO: figure out a way for this to safely wait for any active tasks to finish
	// currently leaks outstanding tasks with no safe way to free them because
	// calling paratask_wait() will try to use self and that'd be use after free

	// possibly add a list of tasks that have finished but haven't been cleaned up?
	// then after we finish waiting for threads to stop we can just walk both task
	// lists and clean them up
	// The list of tasks waiting to be clean up would need to be bi-directional
	// to keep paratask_wait() from having to walk it every time it runs...
	// but if the number of tasks isn't too large that's probably not a problem
	// would need to do something to deal with tasks from paratask_call() since it'll
	// clean them up itself... and  we'll run into problems with it thinking 


	// NOTE:
	// Could set task->work_complete to -1 on all tasks in queue, any active ones
	// should finish before the thread running them exists anyway
	// then we have paratask_call() and paratask_wait() return an error code
	// that indicates the pool shut down and their work wasn't done.

	// Maybe have a count of active tasks and defer cleaning up the context until
	// they complete and have the last one do the clean up?

	mtx_lock(&self->queue_mtx);
	self->shutting_down = 1;
	cnd_broadcast(&self->queue_cnd);

	while(self->nthreads_alive)
		cnd_wait(&self->shutdown_cnd, &self->queue_mtx);
	mtx_unlock(&self->queue_mtx);

	for(int i=0; i<self->nthreads; i++) {
		thrd_join(self->threads[i], NULL);
	}

	mtx_destroy(&self->queue_mtx);
	cnd_destroy(&self->queue_cnd);
	cnd_destroy(&self->shutdown_cnd);
}

int paratask_call(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	// NOTE: this is not actually conformant to C11, C11 doesn't require that
	// you have a full shared address space so things that live on the call stack
	// are allowed to not be addressable on other threads.
	// Maybe fix that, maybe not, since I don't know of any implementation
	// that actually doesn't allow you to address another thread's stack
	struct paratask_task task;

	mtx_lock(&self->queue_mtx);
	launch_task(self, &task, work_offset, work_size, fn, arg);
	task_wait(&task);
	mtx_unlock(&self->queue_mtx);

	return 0;
}

struct paratask_task *paratask_call_async(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	struct paratask_task *task = malloc(sizeof(*task));
	if(!task) return NULL;

	mtx_lock(&self->queue_mtx);
	launch_task(self, task, work_offset, work_size, fn, arg);
	mtx_unlock(&self->queue_mtx);

	return task;
}

void paratask_wait(struct paratask_task *task)
{
	struct paratask_ctx *self = task->ctx;

	mtx_lock(&self->queue_mtx);
	task_wait(task);
	mtx_unlock(&self->queue_mtx);

	free(task);
}

// must be called with queue_mtx held
static void launch_task(struct paratask_ctx *self, struct paratask_task *task, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
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

	cnd_init(&task->idle_cnd);

#if PARATASK_NO_ATOMICS
	mtx_init(&task->work_id_mtx, mtx_plain);
	task->next_work_id = 0;
#else
	atomic_init(&task->next_work_id, 0);
#endif

	if(!self->next_task) {
		assert(self->last_task == NULL);
		self->next_task = self->last_task = task;
	} else {
		self->last_task->next_task = task;
		self->last_task = task;
	}
	cnd_broadcast(&self->queue_cnd);
}

// must be called with queue_mtx held
static void task_wait(struct paratask_task *task)
{
	// need to check both because nthreads_working will ALWAYS be zero the first
	// time through because the workers haven't had a chance to start because we
	// hold queue_mtx, and work_complete gets set when ANY thread runs out stuff
	// to do so it could be set while there's still another worker that hasn't
	// finished it's last work item
	struct paratask_ctx *self = task->ctx;
	while(task->nthreads_working || !task->work_complete)
		cnd_wait(&task->idle_cnd, &self->queue_mtx);
	clear_task(task);
}

static void clear_task(struct paratask_task *task)
{
	task->ctx = NULL;
	task->work_offset = 0;
	task->work_size = 0;
	task->func = NULL;
	task->fn_arg = NULL;

	task->next_task = NULL;
	task->work_complete = 0;
	task->nthreads_working = 0;

	cnd_destroy(&task->idle_cnd);
#if PARATASK_NO_ATOMICS
	mtx_destroy(&task->work_id_mtx);
#endif
}

static size_t next_work_item(struct paratask_task *task)
{
	size_t work_item_id;
	
#if PARATASK_NO_ATOMICS
	mtx_lock(&task->work_id_mtx);
	work_item_id = task->next_work_id++;
	mtx_unlock(&task->work_id_mtx);
#else
	work_item_id = atomic_fetch_add_explicit(&task->next_work_id, 1, memory_order_relaxed);
#endif
	return work_item_id;
}

static int task_thread(void *ctx)
{
	struct paratask_ctx *self = ctx;
	mtx_lock(&self->queue_mtx);
	self->nthreads_alive++;

	char thread_name[16];
	sprintf(thread_name, "paratsk-%04d", self->nthreads_alive);
#if defined(__linux__)
	prctl(PR_SET_NAME, thread_name);
//#elif defined(__APPLE__) && defined(__MACH__)
//	pthread_setname_np(thread_name);
#endif

	while(1) {
		while(!self->next_task && !self->shutting_down) {
			cnd_wait(&self->queue_cnd, &self->queue_mtx);
		}
		if(self->shutting_down) break;

		struct paratask_task *task = self->next_task;

		// if we're the last thread to grab the task or the task is already done
		// remove it from the list
		if(self->nthreads == task->nthreads_working - 1 || task->work_complete) {
			self->next_task = task->next_task;
			if(!self->next_task) self->last_task = NULL;
			continue;
		}

		task->nthreads_working++;

		size_t global_work_offset = task->work_offset;
		size_t global_work_size = task->work_size;
		paratask_task_fn work_func = task->func;
		void *work_fn_arg = task->fn_arg;

		mtx_unlock(&self->queue_mtx);

		while(1) {
			size_t work_item_id = next_work_item(task);

			if(work_item_id >= global_work_size) break;

			work_func(work_item_id + global_work_offset, work_fn_arg);
		}

		mtx_lock(&self->queue_mtx);
		task->work_complete = 1;
		task->nthreads_working--;
		if(!task->nthreads_working) {
			if(thrd_success != cnd_signal(&task->idle_cnd)) abort(); // failure here will almost certainly stall the main thread
		}
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
#elif defined(__NetBSD__)
#include <sched.h>
#include <sys/param.h>
#endif

//TODO: add setting affinity mask in worker threads so that they all
// stick to just one CPU, but only if nthreads was set to default to number
// of cores
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
	cpu_set_t set; //NOTE: will fail on systems with a huge number of cores
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
   return 1; // if we can't get anything just guess
}

