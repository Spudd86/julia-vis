#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <malloc.h>
#include <assert.h>

#include "paratask.h"

#include <threads.h>
#include <stdatomic.h>
#include <stdalign.h>

// We require power of two size
#ifndef PARATASK_QUEUE_LEN_EXP
#define PARATASK_QUEUE_LEN_EXP 7
#endif
#define PARATASK_QUEUE_LEN (1<<PARATASK_QUEUE_LEN_EXP)

// Used for padding and alignment to keep things on different cachelines, no effect on correctness
#ifndef PARATASK_MAX_CACHELINE_SIZE
#	define PARATASK_MAX_CACHELINE_SIZE 128
#endif

#define PT_CL_ALIGN alignas(PARATASK_MAX_CACHELINE_SIZE)

// #define PARATASK_USE_CK_EC 1
#define PARATASK_NO_FUTEX 1

#if __STDC_VERSION__ >= 202311L
#	if __has_c_attribute(gnu::always_inline)
#		define PT_ALWAYS_INLINE [[gnu::always_inline]]
#	endif
#	if __has_c_attribute(gnu::hot)
#		define PT_HOT [[gnu::hot]]
#	endif
#endif

#if defined(__clang_version__) || defined(__GNUC__)
#	ifndef PT_ALWAYS_INLINE
#		define PT_ALWAYS_INLINE __attribute__((always_inline))
#	endif
#	ifndef PT_HOT
#		define PT_HOT __attribute__((hot))
#	endif
#	ifndef likely
#		define likely(x)       __builtin_expect(!!(x), 1)
#	endif
#	ifndef unlikely
#		define unlikely(x)     __builtin_expect(!!(x), 0)
#	endif
#else
#	ifndef PT_ALWAYS_INLINE
#		define PT_ALWAYS_INLINE
#	endif
#	ifndef PT_HOT
#		define PT_HOT
#	endif
#	ifndef likely
#		define likely(x) (x)
#	endif
#	ifndef unlikely
#		define unlikely(x) (x)
#	endif
#endif

/* Future Ideas:
 *
 * - over a certain number of CPUs allocate either per-cpu or per-NUMA domain counters
 *    - once a thread runs out of work on a job it starts using another CPU's counter
 *    - each NUMA domain/CPU has it's own list of the order of other counters it will try to steal from
 *    - could make an array of counters for each CPU, that way it's not so much wasted memory (if there are multiple active jobs)
 */


// TODO: verify this algorithm works and will never deadlock
// TODO: add a "failed to complete" state to barrier and tasks that can be checked and returned from wait functions
// TODO: add a way for callers to discard their tasks and barriers (has to involve the "complete" ec)
// TODO: replace ck_ec with our own version once we're confident the actual algorithm here works

/********************************************************************************************************************************
 * Event count declarations and portability static function prototypes
 */

#if defined(PARATASK_USE_CK_EC)
#include <ck/ck_ec.h>
struct pt_ec {struct ck_ec32 ck_ec;};
#elif !defined(PARATASK_NO_FUTEX)
struct pt_ec {
	atomic_uint_fast32_t counter;
};
#else
struct pt_ec {
	volatile uint32_t counter;
	mtx_t mtx;
	cnd_t cnd;
};
#endif

/// Initialize an event counter, set it's initial value to \param v
static inline void ec_init(struct pt_ec *, uint32_t v) PT_ALWAYS_INLINE;
/// Destroy an event counter
static inline void ec_destroy(struct pt_ec *) PT_ALWAYS_INLINE;
/// Atomically increment the value of the counter and wake any waiters
static inline void ec_inc(struct pt_ec *) PT_ALWAYS_INLINE;
/// Atomically add \param v to the value of the event counter, wake any sleepers. Returns the old value of the counter
static inline uint32_t ec_add(struct pt_ec *, uint32_t v) PT_ALWAYS_INLINE;
/// Wait for the value of an event counter to be differetn from \param old_value
static inline int ec_wait(struct pt_ec *, uint32_t old_value) PT_ALWAYS_INLINE;
/// Return the current value of the event counter
static inline uint32_t ec_value(struct pt_ec *) PT_ALWAYS_INLINE;

/********************************************************************************************************************************
 * Platform sepecific functionality abstraction
 */

static int task_thread(void *);
static int default_thread_count(void);

// For worker threads
static void set_thread_name(int thread_num);
static void set_thread_affinity(int thread_num);

/********************************************************************************************************************************
 * Structs
 */

struct paratask_task
{
	paratask_task_fn func; ///< pointer to kernel function
	void *fn_arg;          ///< data argument for kernel function

	size_t work_offset;   ///< offest to apply to work item ids passed to kernel
	size_t work_size;     ///< total number of work items to execute, work item ids run from [work_offset .. work_offset+work_size)

	PT_CL_ALIGN // Force alignment of next member to push atomics onto a different cacheline

	atomic_size_t next_work_id;  ///< atomic counter of number of completed work items
	struct pt_ec complete;       ///< event counter, counts up to 1 when work is finished, for caller to wait for completion on

	// struct {
	// 	PT_CL_ALIGN
	// 	size_t last_work_item;
	// 	PT_CL_ALIGN atomic_size_t cur_work_item;
	// } per_thread[];
};

struct paratask_barrier
{
	atomic_uint_least16_t threads_arrived; ///< number of threads that have at least reached this barrier
	struct pt_ec all_arrived; ///< all threads wait on this until threads_arrived == nthreads
	struct pt_ec complete;    ///< 0 until threads_passed == nthreads, external waiters wait on this
};

enum paratask_action {
	PT_OP_NONE,
	PT_OP_KERNEL,
	PT_OP_BARRIER,
	//PT_OP_TASK,
	PT_OP_SHUTDOWN,
	PT_OP_INVALID = 15
};

struct paratask_cmd
{
	uint32_t action;
	void *data;
};

struct paratask_thread
{
	PT_CL_ALIGN // Force alignment to put each on it's own cacheline, avoid cache pingpong when threads update their read pointers
	thrd_t tid; ///< Thread id of the worker this struct represents
	struct paratask_ctx *ctx; ///< main paratask instance the worker is part of
	uint32_t thread_num; ///< 0 based numbering, used for looking per-worker things up
	uint32_t work_steal_step;
	atomic_uint_fast32_t queue_read_ptr; ///< this worker's read pointer in the command ringbuffer
};

struct paratask_ctx
{
	unsigned nthreads;       ///< number of threads in our pool
	bool set_affinity;       ///< true if the workers should attempt to set their CPU affinity

	// Command queue, a ringbuffer of commands to workers.
	// Each worker reads every command, though see Command Slot acquire/release below
	// We use a mutex around enqueue actions so we can stick to a single producer implementation, we hope that there's enough
	// work to mitigate any overhead this causes on the producer side.
	// Each worker has it's own read pointer location and at certain times we update the global read pointer to the
	// value of the earliest worker read pointer. So the global one always lags behind and it is always a safe value
	// Global read pointer is updated:
	//   - when all worker threeads are idle
	//   - after completing a barrier command

	struct paratask_cmd commands[PARATASK_QUEUE_LEN];

	/// array of atomic state variables for each ringbuffer slot, wrapped in struct to put each on it's own cacheline
	struct { PT_CL_ALIGN atomic_uint_fast32_t s; } cmd_state[PARATASK_QUEUE_LEN];

	atomic_uint_fast32_t idle_threads;    ///< count of number of worker threads waiting on read slots, use mainly for knowing a good time to advance queue_read_ptr
	atomic_uint_fast32_t queue_read_ptr;  ///< safe read pointer location, actual consumers each have their own. This should be at or before the earliest worker read pointer location
	atomic_uint_fast32_t queue_write_ptr; ///< current global write pointer location
	struct pt_ec queue_serial;            ///< serial number, an event counter we can wait on while queue is empty/full, increments when either queue_read_ptr or queue_write_ptr change.

	mtx_t queue_write_mtx; ///< Used so we can use a single producer design, we care mainly about speed on the worker side

	struct paratask_thread threads[];   ///< all the threads in our pool
};

/*****************************************************************************************************************************
 * Command queue handlers
 */

/* Command Slot acquire/release
 *
 * Command slot acquire/release is needed because the lifetime of the task or barrier struct is not
 * under our direct control. We need to be able to mark the task/barrier complete without later
 * threads walking the queue needing to touch the actual pointed to data object.
 * So we have state tracking within the ringbuffer so we can atomically count how many threads
 * might be looking at the task and mark it completed atomically without a thread sneaking in.
 *
 * We use a uint32_t with the top bit as a complete flag and the bottom 15 as a count of threads
 * aquiring the command, and the next 15 a count of threads releasing it. Done this way since it's
 * easier to figure out what went wrong if there's a bug when you have both counts rather than just a
 * refcount. Since we're counting threads 15 bits is plenty.
 */

/**
 * Fetch a command from the ringbuffer. If the buffer slot is marked completed return a NOOP
 *   = no need to release a NOOP command, it's always already done on return from this function
 */
static inline PT_ALWAYS_INLINE struct paratask_cmd cmd_aquire(struct paratask_ctx *self, uint32_t read_ptr)
{
	uint_fast32_t state = atomic_fetch_add_explicit(&self->cmd_state[read_ptr].s, 1, memory_order_acquire);
	if(state & (UINT32_C(1)<<31)) { // check "complete" flag bit, if set return a no-op command
		return (struct paratask_cmd){ .action = PT_OP_NONE, .data = NULL };
	}
	struct paratask_cmd cmd = self->commands[read_ptr];
	if(PT_OP_NONE == cmd.action) { // just mark no-ops as complete
		atomic_fetch_or_explicit(&self->cmd_state[read_ptr].s, UINT32_C(1)<<31, memory_order_relaxed);
	}
	return cmd;
}

/**
 * "Release" the slot in the buffer by incrementing the number of threads that have "left"
 *  if we were the last one out mark the command itself as done
 *
 * @return true if we were the last one out, so we can mark the job as complete in the task/barrier, false otherwise
 */
static inline PT_ALWAYS_INLINE bool cmd_release(struct paratask_ctx *self, uint32_t read_ptr)
{
	uint_fast32_t state = (1<<15) + atomic_fetch_add_explicit(&self->cmd_state[read_ptr].s, (1<<15), memory_order_release);
	if((state >> 15) == (state&0x7f)) { // number of threads arrived is equal to number of threads that left, we can safely mark complete
		// if this fails another thread must have aquired the command, it will
		// do the completion marking so we can just ignore the failure and return
		return atomic_compare_exchange_strong_explicit(&self->cmd_state[read_ptr].s, &state, state | (1<<31),
		                                               memory_order_release, memory_order_relaxed);
	}
	return false;
}

/**
 * Caclulate number of read slots in ringbuffer with given pointer positions
 * @param w Write pointer position
 * @param r Read pointer position
 * @return number of read slots available
 */
static inline PT_ALWAYS_INLINE uint32_t rb_read_slots(uint32_t w, uint32_t r) {
	if (w > r) {
		return w - r;
	} else {
		return (w + PARATASK_QUEUE_LEN - r) % PARATASK_QUEUE_LEN;
	}
}

/**
 * Caclulate number of write slots in ringbuffer with given pointer positions
 * @param w Write pointer position
 * @param r Read pointer position
 * @return number of write slots available
 */
static inline PT_ALWAYS_INLINE uint32_t rb_write_slots(uint32_t w, uint32_t r) {
	if (w > r) {
		return ((r + PARATASK_QUEUE_LEN - w) % PARATASK_QUEUE_LEN) - 1;
	} else if (w < r) {
		return (r - w) - 1;
	} else {
		return PARATASK_QUEUE_LEN-1;
	}
}

/**
 * Caclulate number of read slots available in the command queue from the given read pointer position
 * @param read_ptr Read pointer position
 * @return number of read slots available
 */
static inline PT_ALWAYS_INLINE uint32_t pt_queue_read_slots(struct paratask_ctx *self, uint32_t read_ptr) {
	uint32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_acquire);
	return rb_read_slots(write_ptr, read_ptr);
}

/**
 * Advance the read pointer one position, requires that there be at least one slot available
 * @param[in,out] read_ptr  Pointer to read pointer to advance
 * @return true if there are still slots available after advancing
 */
static bool pt_advance_read_ptr(struct paratask_ctx *self, uint32_t *read_ptr)
{
	uint32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_relaxed);

	assert(rb_read_slots(write_ptr, *read_ptr));
	*read_ptr = (*read_ptr + 1) % PARATASK_QUEUE_LEN;

	assert(rb_read_slots(write_ptr, *read_ptr) <= rb_read_slots(write_ptr, atomic_load_explicit(&self->queue_read_ptr, memory_order_relaxed)));

	return rb_read_slots(write_ptr, *read_ptr);
}

/**
 * Enqueue a command into the command ringbuffer, wait if there is no space
 * @param op action for workers to take
 * @param data parameters for worker action, pointer to struct paratask_task for PT_OP_KERNEL and pointer to struct paratask_barrier for PT_OP_BARRIER
 */
static int pt_enqueue_command(struct paratask_ctx *self, enum paratask_action op, void *data)
{
	//TODO: Find a lock free way that is also fast
	mtx_lock(&self->queue_write_mtx);

	uint32_t serial = ec_value(&self->queue_serial);
	uint_fast32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_relaxed);

	// Wait for space
	uint32_t read_ptr = atomic_load_explicit(&self->queue_read_ptr, memory_order_relaxed);
	while(0 == rb_write_slots(write_ptr, read_ptr)) {
		ec_wait(&self->queue_serial, serial);
		serial = ec_value(&self->queue_serial);
		read_ptr = atomic_load_explicit(&self->queue_read_ptr, memory_order_relaxed);
	}

	self->commands[write_ptr].action = op;
	self->commands[write_ptr].data = data;
	atomic_store_explicit(&self->cmd_state[write_ptr].s, 0, memory_order_relaxed);

	assert(rb_write_slots((write_ptr + 1) % PARATASK_QUEUE_LEN, read_ptr) < rb_write_slots(write_ptr, read_ptr));

	// Lock makes us act like a single producer, so we don't need to worry about anything else changing the write pointer
	// so just blindly store our new calculated value
	atomic_store_explicit(&self->queue_write_ptr, (write_ptr + 1) % PARATASK_QUEUE_LEN, memory_order_release);
	// Can't use a CAS loop here because of the write to the command buffer. If we write before a CAS we overwrite the values
	// from a concurrent enqueue that got the same value for the write pointer, if we write after the CAS workers might
	// see the incorrect values
	mtx_unlock(&self->queue_write_mtx);
	ec_inc(&self->queue_serial);

	return 0;
}

/**
 * Wait for commands to run to be available in the command ringbuffer from the given read pointer position
 * @param read_ptr the read pointer position to use
 */
static void pt_wait_read_ptr(struct paratask_ctx *self, uint32_t read_ptr)
{
	uint32_t serial = ec_value(&self->queue_serial);
	uint32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_acquire);

	// global read pointer should never be ahead of us
	assert(rb_read_slots(write_ptr, read_ptr) <= rb_read_slots(write_ptr, self->queue_read_ptr));

	while(0 == rb_read_slots(write_ptr, read_ptr)) {
		ec_wait(&self->queue_serial, serial);
		serial = ec_value(&self->queue_serial);
		write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_acquire);
	}
}

/** Helper only, does the CAS loop for updating the global ringbuffer read pointer without letting it ever go backwards */
static inline PT_ALWAYS_INLINE void pt_set_glbl_read_ptr_impl(struct paratask_ctx *self, uint32_t new_read_ptr, uint32_t write_ptr) {
	// WARNING: this only works because we only update global read pointer from work threads
	//       - works because we can rely on the write pointer not wrapping past this worker's
	//         read pointer (new_glbl_read_ptr is either equal to this worker's read pointer
	//         or it's closer to write pointer, either way the write pointer won't pass it while
	//         we are working since we aren't updating our shared read pointer atomic)

	// Would also be safe under the mutex for the write side since we can then count on the write pointer not moving

	// The main hazzard is if this runs in another thread then the write pointer advances past one of
	// the values we read. That is we're mainly worried about out of order visibility of write pointer vs read pointers

	uint_fast32_t read_ptr = atomic_load_explicit(&self->queue_read_ptr, memory_order_relaxed);
	while( read_ptr != new_read_ptr
	        && rb_read_slots(write_ptr, new_read_ptr) < rb_read_slots(write_ptr, read_ptr)) {
		if(atomic_compare_exchange_weak_explicit(&self->queue_read_ptr, &read_ptr, new_read_ptr,
		                                         memory_order_relaxed, memory_order_relaxed))
			break;
	}
	ec_inc(&self->queue_serial);
}

/**
 * Set the global read pointer to the given value
 * @param read_ptr the new value for the global ringbuffer read position
 */
static void pt_set_glbl_read_ptr(struct paratask_ctx *self, uint32_t read_ptr) {
	uint32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_relaxed);
	pt_set_glbl_read_ptr_impl(self, read_ptr, write_ptr);
}

/**
 * Advance the global read pointer to the next safe value based on per-thread positions.
 */
static void pt_advance_glbl_read_ptr(struct paratask_ctx *self)
{
	uint32_t write_ptr = atomic_load_explicit(&self->queue_write_ptr, memory_order_relaxed);
	uint_fast32_t glbl_read_ptr = atomic_load_explicit(&self->queue_read_ptr, memory_order_relaxed);
	uint32_t new_glbl_read_ptr = glbl_read_ptr;
	int64_t high_slots = -1;

	for(size_t i=0; i < self->nthreads; i++) {
		uint32_t read_ptr = atomic_load_explicit(&self->threads[i].queue_read_ptr, memory_order_relaxed);
		uint32_t slots = rb_read_slots(write_ptr, read_ptr);
		if(slots > high_slots) {
			new_glbl_read_ptr = read_ptr;
			high_slots = slots;
		}
	}
	pt_set_glbl_read_ptr_impl(self, new_glbl_read_ptr, write_ptr);
}

/*****************************************************************************************************************************
 * Main API functions
 */

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

	mtx_init(&self->queue_write_mtx, mtx_plain);

	atomic_init(&self->queue_read_ptr, 0);
	atomic_init(&self->queue_write_ptr, 0);
	ec_init(&self->queue_serial, 0);
	atomic_init(&self->idle_threads, 0);

	for(size_t i=0; i<PARATASK_QUEUE_LEN; i++) {
		self->commands[i].action = PT_OP_INVALID;
		self->commands[i].data = NULL;
		atomic_init(&self->cmd_state[i].s, 1<<31);
	}

	for(size_t i=0; i < self->nthreads; i++) {
		self->threads[i].ctx = self;
		self->threads[i].thread_num = i;
		self->threads[i].work_steal_step = (i%2) ? (self->nthreads-1) : 1; // odd threads go backwards
		atomic_init(&self->threads[i].queue_read_ptr, 0);
	}

	for(size_t i=0; i < self->nthreads; i++) {
		int err = thrd_create(&self->threads[i].tid, task_thread, &self->threads[i]);
		if(err != thrd_success) {
			if(err == thrd_nomem) fprintf(stderr, "Failed to create thread, no memory\n");
			else if(err == thrd_error) fprintf(stderr, "Failed to create thread\n");
			else fprintf(stderr, "Failed to create thread %i\n", err);
			abort(); //FIXME: do better than this
		}
	}

	return self;
}

void paratask_delete(struct paratask_ctx *self)
{
	// FIXME: check error
	pt_enqueue_command(self, PT_OP_SHUTDOWN, NULL);

	// Wait for all threads
	for(size_t i=0; i<self->nthreads; i++) {
		thrd_join(self->threads[i].tid, NULL);
	}

	ec_destroy(&self->queue_serial);

	free(self);
}

int paratask_call(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	// If only 1 thread and we're not going to be async just run in current context
	// so we don't have threading overhead when it won't buy us anything.
	if(self->nthreads == 1)
	{
		//FIXME: return errors instead of calling abort()
		if(work_size >= SIZE_MAX - work_offset) abort();
		if(work_size >= SIZE_MAX - self->nthreads - 1) abort(); // avoid overflow in picking work item ids in task thread

		for(size_t gid = 0; gid < work_size; gid++) {
			fn(gid + work_offset, arg);
		}
		return 0;
	}

	struct paratask_task *task = paratask_call_async(self, work_offset, work_size, fn, arg);
	if(!task) return -1;

	paratask_wait(task);

	return 0;
}

struct paratask_task *paratask_call_async(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg)
{
	// struct paratask_task *task = malloc(sizeof(*task) + sizeof(*task->per_thread)*self->nthreads);
	struct paratask_task *task = malloc(sizeof(*task));
	if(!task) return NULL;

	//FIXME: return errors instead of calling abort()
	// not too worried about hitting these though because that's a truly unreasonable
	// amount of work, especially on 64 bit systems
	if(work_size >= SIZE_MAX - work_offset) abort();
	if(work_size >= SIZE_MAX - self->nthreads - 1) abort(); // avoid overflow in picking work item ids in task thread

	task->work_offset = work_offset;
	task->work_size = work_size;
	task->func = fn;
	task->fn_arg = arg;

	atomic_init(&task->next_work_id, 0);
	ec_init(&task->complete, 0);

	// size_t min_work_size = work_size / (size_t)self->nthreads;
	// size_t extra_work = work_size % (size_t)self->nthreads;
	// size_t cur_first_item = 0;
	// for(size_t i=0; i<self->nthreads; i++) {
	// 	atomic_init(&task->per_thread[i].cur_work_item, cur_first_item);
	// 	cur_first_item += min_work_size;
	// 	if(extra_work > 0) {
	// 		extra_work--;
	// 		cur_first_item++;
	// 	}
	// 	task->per_thread[i].last_work_item = cur_first_item;
	// 	// task->per_thread[i].last_work_item = (work_size * (i + 1))/self->nthreads;
	// 	// atomic_init(&task->per_thread[i].cur_work_item, (work_size * i)/self->nthreads);
	// }

	// TODO: check error
	pt_enqueue_command(self, PT_OP_KERNEL, task);

	return task;
}

int paratask_wait(struct paratask_task *task)
{
	while(!ec_value(&task->complete)) {
		ec_wait(&task->complete, 0);
	}

	int res = ec_value(&task->complete);

	ec_destroy(&task->complete);

	free(task);

	if(res == 1) return 0;
	return 1;
}

struct paratask_barrier *paratask_enqueue_barrier(struct paratask_ctx *self)
{
	struct paratask_barrier *barrier = malloc(sizeof(*barrier));
	atomic_init(&barrier->threads_arrived, 0);
	ec_init(&barrier->all_arrived, 0);
	ec_init(&barrier->complete, 0);

	// TODO: check error
	pt_enqueue_command(self, PT_OP_BARRIER, barrier);

	return 0;
}

int paratask_wait_barrier(struct paratask_barrier *barrier)
{
	while(!ec_value(&barrier->complete)) {
		ec_wait(&barrier->complete, 0);
	}
	int res = ec_value(&barrier->complete);

	ec_destroy(&barrier->all_arrived);
	ec_destroy(&barrier->complete);
	// TODO: timeout version?
	free(barrier);

	if(res == 1) return 0;
	return 1;
}


/************************************************************************************************************************************************
 * Task thread and it's helpers
 */

struct work_item_state
{
	struct paratask_thread *thread;
	struct paratask_task *task;
	unsigned last_work_threadnum;
};
static inline void init_work_item_state(struct work_item_state *state, struct paratask_thread *thread, struct paratask_task *task)
{
	state->thread = thread;
	state->task = task;
	state->last_work_threadnum = thread->thread_num;
}

static PT_HOT inline PT_ALWAYS_INLINE size_t next_work_item(struct work_item_state *state)
{
	size_t work_item_id;
	work_item_id = atomic_fetch_add_explicit(&state->task->next_work_id, 1, memory_order_relaxed);

	// while(1) {
	// 	work_item_id = atomic_fetch_add_explicit(&state->task->per_thread[state->last_work_threadnum].cur_work_item, 1, memory_order_relaxed);
	// 	if(unlikely(work_item_id >= state->task->per_thread[state->last_work_threadnum].last_work_item)) {
	// 		state->last_work_threadnum = (state->last_work_threadnum + state->thread->work_steal_step) % state->thread->ctx->nthreads;
	// 		if(unlikely(state->last_work_threadnum == state->thread->thread_num)) {
	// 			return state->task->work_size;
	// 		}
	// 		continue;
	// 	}
	// 	break;
	// }

	return work_item_id;
}

static PT_HOT int task_thread(void *ctx)
{
	struct paratask_thread *thread = ctx;
	struct paratask_ctx *self = thread->ctx;
	int thread_num = thread->thread_num;

	set_thread_name(thread_num);

	if(self->set_affinity)
		set_thread_affinity(thread_num);

	bool shutting_down = false;
	uint32_t queue_read_ptr = self->queue_read_ptr;
	while(!shutting_down)
	{
		// if all threads are idle advance the global read pointer
		if(atomic_fetch_add_explicit(&self->idle_threads, 1, memory_order_acq_rel) + 1u == self->nthreads) {
			pt_advance_glbl_read_ptr(self);
		}

		pt_wait_read_ptr(self, queue_read_ptr);

		// Not idle anymore
		atomic_fetch_sub_explicit(&self->idle_threads, 1, memory_order_relaxed);

		// Loop until we run out of commands
		while(pt_queue_read_slots(self, queue_read_ptr))
		{
			bool set_global_read_ptr = false;
			struct paratask_cmd cmd = cmd_aquire(self, queue_read_ptr);
			switch(cmd.action)
			{
				case PT_OP_KERNEL: {
					// TODO: figure out how to force these to actually happen since the actual task struct will be very cache expensive to look at
					struct paratask_task *task = cmd.data;
					size_t global_work_offset = task->work_offset;
					size_t global_work_size = task->work_size;
					paratask_task_fn work_func = task->func;
					void *work_fn_arg = task->fn_arg;

					struct work_item_state work_item_state;
					init_work_item_state(&work_item_state, thread, task);

					size_t work_item_id = next_work_item(&work_item_state);
					while(work_item_id < global_work_size) {
						work_func(work_item_id + global_work_offset, work_fn_arg);
						work_item_id = next_work_item(&work_item_state);
					}

					if(cmd_release(self, queue_read_ptr)) { // we're the last worker out, mark job done
						ec_inc(&task->complete);
					}
				} break;

				case PT_OP_BARRIER: {
					struct paratask_barrier *barrier = cmd.data;
					// Need to add again since atomic op returns old value of the atomic
					uint16_t narrived = 1 + atomic_fetch_add_explicit(&barrier->threads_arrived, 1, memory_order_relaxed);

					if(narrived == self->nthreads) { // last arriver, mark the event
						ec_inc(&barrier->all_arrived);
					}

					while(narrived < self->nthreads) {
						ec_wait(&barrier->all_arrived, 0);
						narrived = atomic_load_explicit(&barrier->threads_arrived, memory_order_relaxed);
					}

					if(cmd_release(self, queue_read_ptr)) { // we're the last worker out, mark job done
						ec_inc(&barrier->complete);
						// we know all threads are past this so we can just set
						// the global read pointer to ours after we advance
						set_global_read_ptr = true;
					}
				} break;

				case PT_OP_NONE: break; // literally do nothing, not even releasing the command slot

				default:
					assert(false);
				case PT_OP_SHUTDOWN:
					shutting_down = true;
					break;
			}

			pt_advance_read_ptr(self, &queue_read_ptr);
			atomic_store_explicit(&self->threads[thread_num].queue_read_ptr, queue_read_ptr, memory_order_relaxed);
			if(set_global_read_ptr) {
				pt_set_glbl_read_ptr(self, queue_read_ptr);
			}
		}
	}

	return 0;
}

/*****************************************************************************************************************************
 * Default instance
 */

static once_flag default_instance_once_flag = ONCE_FLAG_INIT;
static struct paratask_ctx *default_instance = NULL;
static void create_default_instance(void) { default_instance = paratask_new(0); }
struct paratask_ctx *paratask_default_instance(void) {
	call_once(&default_instance_once_flag, create_default_instance);
	return default_instance;
}

/************************************************************************************************************************************************
 * Platform specific code
 */

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

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
#if _WIN32_WINNT >= 601
// TODO: use GetProcessGroupAffinity and SetThreadGroupAffinity along with the Mask functions below
// Or use https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets
#endif
// these only work properly on systems with less than 64 CPUs, but it's unlikely
// that this code will scale up to systems even that large anyway
// It will however properly use the 64 CPUs it can find
static void set_thread_affinity(int thread_num) {
	DWORD_PTR process_mask;
	DWORD_PTR system_mask;
	if(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask))
	{
		int count = 0;
		for(size_t i=0; i<64; i++) {
			if((process_mask >> i) & 1)
			{
				if(count == thread_num) {
					DWORD_PTR mask = 1;
					mask = mask << i;
					SetThreadAffinityMask(GetCurrentThread(), mask);
					return;
				}
				count++;
			}
		}
	}
}
static int default_thread_count(void) {
	DWORD_PTR process_mask;
	DWORD_PTR system_mask;
	if(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
		DWORD_PTR mask = process_mask;
		for(int count = 0; mask != 0; mask = mask >> 1) {
			if (mask & 1) count++;
		}
		return count;
	}
	return 2; // if we can't get anything just guess
}

#elif defined(__linux__) //exists in glibc >= 2.3.4 and in musl, Linux specific syscalls
#include <sched.h>
static void set_thread_affinity(int thread_num) {
	cpu_set_t set; //NOTE: will fail on systems with a huge number of cores (>1024), but this probably won't scale to that anyway
	if(sched_getaffinity(0, sizeof(set), &set) != 0) return;
	int count = 0;
	for(size_t i=0; i<CPU_SETSIZE; i++) {
		if(CPU_ISSET(i, &set)) {
			if(count == thread_num) {
				cpu_set_t newset; CPU_ZERO(&newset);
				CPU_SET(i, &newset);
				sched_setaffinity(0, sizeof(newset), &newset); // we can't really do anything if this fails so just ignore that
				return;
			}
			count++;
		}
	}
}
static int default_thread_count(void) {
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
	return 4;
}
#elif defined(__NetBSD__)
#include <sched.h>
#include <pthread.h>
static void set_thread_affinity(int thread_num)
{
	cpuset_t *cset = cpuset_create();
	pthread_t tid = pthread_self();
	if(pthread_getaffinity_np(tid, cpuset_size(cset), cset) == 0) {
		int count = 0;
		for(size_t cpu = 0; cpu < cpulimit; cpu++) {
			if (cpuset_isset(cpu, cpuset)) {
				if(count == thread_num) {
					cpuset_zero(cset);
					cpuset_set(cpu, cset);
					pthread_getaffinity_np(tid, cpuset_size(cset), cset);
					break;
				}
				count++;
			}
		}
	}
	cpuset_destroy(cset);
}
static int default_thread_count(void)
{
	cpuset_t *cset = cpuset_create();
	if(sched_getaffinity_np(getpid(), cpuset_size(cset), cset)) {
		cpuset_destroy(cset);
		return 4; // failed
	}
	size_t max_cpus = cpuset_size(cset) * CHAR_BIT;
	int ncpus = 0;
	for(size_t cpu = 0; cpu < cpulimit; cpu++) {
		if (cpuset_isset(cpu, cpuset)) ncpus++;
	}
	cpuset_destroy(cset);
	return ncpus;
}
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sysexits.h>
static int default_thread_count(void) {
	cpuset_t cpuset; CPU_ZERO(&cpuset);
	if(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(cpuset), &cpuset) == 0) {
		return CPU_COUNT(&cpuset);
	}
	return 4;
}
static void set_thread_affinity(int thread_num) {
	cpuset_t cpuset; CPU_ZERO(&cpuset);
	if(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(cpuset), &cpuset) != 0) return;
	int count = 0;
	for(size_t i=0; i<CPU_SETSIZE; i++) {
		if(CPU_ISSET(i, &set)) {
			if(count == thread_num) {
				cpu_set_t newset; CPU_ZERO(&newset);
				CPU_SET(i, &newset);
				cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(newset), &newset);
				return;// we can't really do anything if this fails so just ignore that
			}
			count++;
		}
	}
}
#elif defined(__APPLE__) && defined(__MACH__)
#include <sys/types.h>
#include <sys/sysctl.h>
static int default_thread_count(void) {
	int mib[2] = {CTL_HW, HW_NCPU};
	int ncpu;
	size_t len = sizeof(ncpu);
	if(0 == sysctl(mib, 2, &ncpu, &len, NULL, 0)) {
		return ncpu;
	}
	return 4;
}
static void set_thread_affinity(int thread_num) {} //TODO:
#elif defined(__EMSCRIPTEN__)
#include <emscripten/wasm_worker.h>
static int default_thread_count(void) {
	return emscripten_navigator_hardware_concurrency();
}
static void set_thread_affinity(int thread_num) {} //TODO:
#else
static void set_thread_affinity(int thread_num) {}
static int default_thread_count(void) {
#if defined(_SC_NPROCESSORS_ONLN)
	return sysconf(_SC_NPROCESSORS_ONLN);
#else
	return 2; // if we can't get anything just guess
#endif
}
#endif

/*****************************************************************************************************************************************
 * Event counter support/implementation
 */

#if defined(PARATASK_USE_CK_EC) && !defined(PARATASK_NO_FUTEX)

static int gettime(const struct ck_ec_ops *ops, struct timespec *tv);

#define MS_PER_SEC      UINT64_C(1000)     // MS = milliseconds
#define US_PER_MS       UINT64_C(1000)     // US = microseconds
#define HNS_PER_US      UINT64_C(10)       // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US       UINT64_C(1000)
#define HNS_PER_SEC     (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS      UINT64_C(100ULL)    // NS = nanoseconds
#define NS_PER_SEC      (MS_PER_SEC * US_PER_MS * NS_PER_US)
#define NS_PER_MS       (US_PER_MS * NS_PER_US)

static struct timespec diff_timespec(const struct timespec *time1,
                                     const struct timespec *time0) {
	struct timespec diff = {
		.tv_sec = time1->tv_sec - time0->tv_sec,
		.tv_nsec = time1->tv_nsec - time0->tv_nsec
	};
	if (diff.tv_nsec < 0) {
		diff.tv_nsec += NS_PER_SEC;
		diff.tv_sec--;
	}
	return diff;
}

static uint64_t ts_absolute_to_relative_ms(const struct timespec *deadline) {
	struct timespec now, diff;
	gettime(NULL, &now);
	diff = diff_timespec(deadline, &now);
	return diff.tv_sec * MS_PER_SEC + diff.tv_nsec/NS_PER_MS;
}

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)

static int gettime(const struct ck_ec_ops *ops, struct timespec *tv) {
	LARGE_INTEGER ticksPerSec;
	LARGE_INTEGER ticks;
	QueryPerformanceFrequency(&ticksPerSec);
	if (!ticksPerSec.QuadPart) return -1;
	QueryPerformanceCounter(&ticks);
	tv->tv_sec = (long)(ticks.QuadPart / ticksPerSec.QuadPart);
	tv->tv_nsec = (long)(((ticks.QuadPart % ticksPerSec.QuadPart) * NS_PER_SEC) / ticksPerSec.QuadPart);
	return 0;
}

static void wait32(const struct ck_ec_wait_state *state,
                   const uint32_t *address, uint32_t expected,
                   const struct timespec *deadline) {
	WaitOnAddress(address, &expected, sizeof(*address), (deadline?(DWORD)ts_absolute_to_relative_ms(deadline):INFINITE));
}
static void wake32(const struct ck_ec_ops *ops, const uint32_t *address) { WakeByAddressAll(address); }

#elif defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>

static int gettime(const struct ck_ec_ops *ops, struct timespec *out) {(void)ops;
	return clock_gettime(CLOCK_MONOTONIC, out);
}
static void wait32(const struct ck_ec_wait_state *state,
		   const uint32_t *address, uint32_t expected,
		   const struct timespec *deadline) {(void)state;
	syscall(SYS_futex, address,
		FUTEX_WAIT_BITSET, expected, deadline,
		NULL, FUTEX_BITSET_MATCH_ANY, 0);
}
static void wake32(const struct ck_ec_ops *ops, const uint32_t *address) {(void)ops;
	syscall(SYS_futex, address,
		FUTEX_WAKE, INT_MAX,
		/* ignored arguments */NULL, NULL, 0);
}
#elif defined(__OpenBSD__)
#include <sys/time.h>
#include <sys/futex.h>
static int gettime(const struct ck_ec_ops *ops, struct timespec *out) {(void)ops;
	return clock_gettime(CLOCK_MONOTONIC, out);
}
static void wait32(const struct ck_ec_wait_state *state,
                   const uint32_t *address, uint32_t expected,
                   const struct timespec *deadline) {(void)state;
	struct timespec diff;
	if(deadline) {
		struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
		diff = diff_timespec(deadline, &now);
	}
	futex(address, FUTEX_WAIT, expected, deadline?&diff:NULL, NULL);
}
static void wake32(const struct ck_ec_ops *ops, const uint32_t *address) {(void)ops;
	futex(address, FUTEX_WAKE, INT_MAX, NULL, NULL);
}
#elif  defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/umtx.h>
static int gettime(const struct ck_ec_ops *ops, struct timespec *out) {(void)ops;
	return clock_gettime(CLOCK_MONOTONIC, out);
}
static void wait32(const struct ck_ec_wait_state *state,
                   const uint32_t *address, uint32_t expected,
                   const struct timespec *deadline) {(void)state;
	struct _umtx_time = {
		._timeout = (deadline)?*deadline:(struct timespec){0, 0},
		._flags   = UMTX_ABSTIME,
		._clockid = CLOCK_MONOTONIC
	}timeout;
	_umtx_op(address, UMTX_OP_WAIT, expected, NULL, deadline?&timeout:NULL);
}
static void wake32(const struct ck_ec_ops *ops, const uint32_t *address) {(void)ops;
	_umtx_op(address, UMTX_OP_WAKE, INT_MAX, NULL, NULL);
}
#elif defined(__DragonFly__)
static int gettime(const struct ck_ec_ops *ops, struct timespec *out) {(void)ops;
	return clock_gettime(CLOCK_MONOTONIC, out);
}
static void wait32(const struct ck_ec_wait_state *state,
                   const uint32_t *address, uint32_t expected,
                   const struct timespec *deadline) {(void)state;
	umtx_sleep(address, expected, (deadline?(int)ts_absolute_to_relative_ms(deadline):0));
}
static void wake32(const struct ck_ec_ops *ops, const uint32_t *address) {(void)ops; umtx_wakeup(address, INT_MAX); }
#endif

static const struct ck_ec_ops paratask_ec_ops = {
	.gettime = gettime,
	.wait32 = wait32,
	.wake32 = wake32,
	.wait64 = NULL,
	.wake64 = NULL
};
static const struct ck_ec_mode paratask_ec_mode = {
	.ops             = &paratask_ec_ops,
	.single_producer = false,
};
static void ec_destroy(struct pt_ec *ec) {(void)ec;}
static void ec_init(struct pt_ec *ec, uint32_t v) { ck_ec_init(&ec->ck_ec, v); }
static void ec_inc(struct pt_ec *ec) { ck_ec_inc(&ec->ck_ec, &paratask_ec_mode); }
static uint32_t ec_add(struct pt_ec *ec, uint32_t v) { return ck_ec_add(&ec->ck_ec, &paratask_ec_mode, v); }
static uint32_t ec_value(struct pt_ec *ec) { return ck_ec_value(&ec->ck_ec); }
static int ec_wait(struct pt_ec *ec, uint32_t old_value) { return ck_ec_wait(&ec->ck_ec, &paratask_ec_mode, old_value, NULL); }

#elif !defined(PARATASK_NO_FUTEX)
/*  The following is based on ck_ec.h from concurrencykit it's cut down to just do what is needed for Paratask
 */

// See EventCounts here https://web.archive.org/web/20120428210745/https://locklessinc.com/articles/obscure_synch/

// For older Windows, and potentially other places
// https://blog.bearcats.nl/atomic-wait/
// https://web.archive.org/web/20190616035409/http://locklessinc.com/articles/keyed_events/

// See https://doc.rust-lang.org/nightly/src/std/sys/pal/unix/futex.rs.html
// https://man.dragonflybsd.org/?command=umtx&section=2
// https://shift.click/blog/futex-like-apis/

//TODO: WASM version (has futex like things!)
//TODO: Mac with os_sync_wait_on_address/os_sync_wake_by_address_all

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
static void wait32(const void *address, uint32_t expected) {
	WaitOnAddress(address, &expected, sizeof(*address), INFINITE));
}
static void wake32(const void *address) {
	WakeByAddressAll(address);
}
#elif defined(__APPLE__) && defined(__MACH__)
// Available starting with macOS 14.4
// For older MacOS https://github.com/m-ou-se/atomic-wait/blob/main/src/macos.rs or https://github.com/thomcc/ulock-sys/blob/main/src/lib.rs
#include <os/os_sync_wait_on_address.h>
static void wait32(const void *address, uint32_t expected) {
	s_sync_wait_on_address(address, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE);
}
static void wake32(const void *address) {
	os_sync_wake_by_address_all(address, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE);
}

#elif defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
static void wait32(const void *address, uint32_t expected) {
	syscall(SYS_futex, address, FUTEX_WAIT_BITSET, expected, NULL, NULL, FUTEX_BITSET_MATCH_ANY, 0);
}
static void wake32(const void *address) {
	syscall(SYS_futex, address, FUTEX_WAKE, INT_MAX, /* ignored arguments */NULL, NULL, 0);
}
#elif defined(__OpenBSD__)
#include <sys/futex.h>
static void wait32(const void *address, uint32_t expected) {
	futex(address, FUTEX_WAIT, expected, NULL, NULL);
}
static void wake32(const uint32_t *address) {
	futex(address, FUTEX_WAKE, INT_MAX, NULL, NULL);
}
#elif  defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/umtx.h>
static void wait32(const void *address, uint32_t expected) {
	_umtx_op(address, UMTX_OP_WAIT, expected, NULL, NULL);
}
static void wake32(const void *address) {
	_umtx_op(address, UMTX_OP_WAKE, INT_MAX, NULL, NULL);
}
#elif defined(__DragonFly__)
static void wait32(const void *address, uint32_t expected) {
	umtx_sleep(address, expected, 0));
}
static void wake32(const void *address) { umtx_wakeup(address, INT_MAX); }
#elif defined(__EMSCRIPTEN__)
#include <math.h>
#include <emscripten/threading.h>
#include <emscripten/wasm_worker.h>
static void wait32(const void *address, uint32_t expected) {
	emscripten_futex_wait(address, expected, HUGE_VAL);
}
static void wake32(const void *address) { emscripten_futex_wake(address, INT_MAX); }
#else
#	error "Futex based Paratask eventcount implemention selected, but platform doesn't support a futex-like syscall"
#endif

#define EC_FLAG_MASK (UINT32_C(1) << 31)
#define EC_VALUE_MASK (~EC_FLAG_MASK)

static inline void ec_destroy(struct pt_ec *) {}
static inline void ec_init(struct pt_ec *ec, uint32_t v) {
	atomic_init(&ec->counter, v & EC_VALUE_MASK);
}

static inline uint32_t ec_value(struct pt_ec *ec) {
	return atomic_load_explicit(&ec->counter, memory_order_relaxed)  & ~(UINT32_C(1) << 31);
}

//TODO: work out the equivalent barriers to what ck_ec uses more closely
//TODO: currently the mutex and condition variable version seems to be faster! FIX THAT!

static inline uint32_t ec_add(struct pt_ec *ec, uint32_t v) {
	atomic_thread_fence(memory_order_release);
	uint32_t old = atomic_fetch_add_explicit(&ec->counter, v, memory_order_relaxed);
	uint32_t ret = old & EC_VALUE_MASK;
	/* These two only differ if the flag bit is set. */
	if (unlikely(old != ret)) {
		/* Spurious wake-ups are OK. Clear the flag before futexing. */
		atomic_fetch_and_explicit(&ec->counter, EC_VALUE_MASK, memory_order_relaxed);
		wake32(&ec->counter);
	}
	return ret;
}
static inline void ec_inc(struct pt_ec *ec) {
	ec_add(ec, 1);
}

static inline PT_ALWAYS_INLINE uint32_t ec_wait_easy(struct pt_ec *ec, uint32_t expected) {
	uint32_t current = atomic_load_explicit(&ec->counter, memory_order_relaxed);
	static const size_t n = 100U;

	for (size_t i = 0; i < n && current == expected; i++) { // busy wait
		current = atomic_load_explicit(&ec->counter, memory_order_relaxed);
	}
	return current;
}

static inline PT_ALWAYS_INLINE bool ec_upgrade(struct pt_ec *ec, uint32_t current, uint32_t unflagged, uint32_t flagged) {
	uint_fast32_t old_word = unflagged;
	if (current == flagged) return false;
	if (current != unflagged) return true;
	return (!atomic_compare_exchange_weak_explicit(&ec->counter, &old_word, flagged, memory_order_relaxed, memory_order_relaxed))
	        && (old_word != flagged);
}

static int ec_wait_slow(struct pt_ec *ec, uint32_t old_value)
{
	const uint32_t flagged_value = old_value | (1UL << 31);
	while(1) {
		uint32_t current = ec_wait_easy(ec, old_value);

		// We're about to wait harder (i.e., potentially with futex). Make sure the counter word is flagged.
		if (likely(ec_upgrade(ec, current, old_value, flagged_value))) {
			atomic_thread_fence(memory_order_acquire);
			return 0;
		}

		// By now, ec->counter == flagged_word (at some point in the past). Spin some more to heuristically let any in-flight inc/add
		// to retire. This does not affect correctness, but practically eliminates lost wake-ups.
		current = ec_wait_easy(ec, flagged_value);
		if (likely(current != flagged_value)) {
			atomic_thread_fence(memory_order_acquire);
			return 0;
		}

		// TODO: add exponential backoff?
		wait32((void *)&ec->counter, flagged_value);

		current = atomic_load_explicit(&ec->counter, memory_order_relaxed) & ~(UINT32_C(1) << 31);
		if(likely(current != old_value)) {
			atomic_thread_fence(memory_order_acquire);
			return 0;
		}
		/* Spurious wake-up. Redo the slow path. */
	}
	return 0;
}

static inline int ec_wait(struct pt_ec *ec, uint32_t old_value) {
	if (likely(atomic_load_explicit(&ec->counter, memory_order_relaxed) != old_value)) {
		return 0;
	}
	return ec_wait_slow(ec, old_value);
}
#else
static inline void ec_init(struct pt_ec *ec, uint32_t v) {
	ec->counter = v;
	mtx_init(&ec->mtx, mtx_plain);
	cnd_init(&ec->cnd);
}
static inline void ec_destroy(struct pt_ec *ec) {
	mtx_destroy(&ec->mtx);
	cnd_destroy(&ec->cnd);
}
static inline uint32_t ec_value(struct pt_ec *ec) {
	uint32_t r;
	r = ec->counter;
	return r;
}
static inline void ec_inc(struct pt_ec *ec) {
	ec_add(ec, 1);
}
static inline uint32_t ec_add(struct pt_ec *ec, uint32_t v) {
	uint32_t r;
	mtx_lock(&ec->mtx);
	r = ec->counter;
	ec->counter += v;
	mtx_unlock(&ec->mtx);
	cnd_broadcast(&ec->cnd);
	return r;
}
static inline int ec_wait(struct pt_ec *ec, uint32_t old_value) {
	mtx_lock(&ec->mtx);
	while(ec->counter == old_value) {
		cnd_wait(&ec->cnd, &ec->mtx);
	}
	mtx_unlock(&ec->mtx);
	return 0;
}
#endif
