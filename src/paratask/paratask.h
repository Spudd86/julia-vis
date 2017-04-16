#ifndef PARATASK_H_
#define PARATASK_H_

/* A thread pool, with a twist.
 *
 * The work of doing the job is split across
 * multiple threads. Each thread has a task number that identifies a segment of work
 * it to do. It's somewhat like a "Work item" in OpenCL.
 */

struct paratask_ctx;
struct paratask_task;

typedef void (*paratask_task_fn)(size_t work_item_id, void *arg);

/**
 * @nthreads number of threads to use for running tasks, 0 is default and will
 * attempt to detect the number of available cores and create that many threads
 * a negative number will cause it to create min(abs(nthreads), number_of_cores)
 */
struct paratask_ctx *paratask_new(int nthreads);
struct paratask_ctx *paratask_default_instance(void);


void paratask_delete(struct paratask_ctx *self);

int paratask_call(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg);

struct paratask_task *paratask_call_async(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg);

/** Wait for the task to finish, also frees the task.
 *  safe to call from multiple threads with tasks from the same context
 *  NOT safe to call multiple times with the same task pointer
 *
 *  Returns non-zero if the work was terminated before every work item was run
 */
int paratask_wait(struct paratask_task *task);

#endif