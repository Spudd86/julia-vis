#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include "paratask.h"
#include "tinycthread.h"

struct work_fn_args
{
	uint32_t *out;
	size_t block_size;
};

static void work_function(size_t work_item_id, void *args_)
{
	struct work_fn_args *args = args_;
	uint32_t *out = args->out;
	size_t block_size = args->block_size;

	for(size_t i = work_item_id*block_size; i < (work_item_id + 1)*block_size; i++) {
		out[i] = i;
	}
}

static void work_function2(size_t work_item_id, void *args_)
{
	struct work_fn_args *args = args_;
	uint32_t *out = args->out;
	size_t block_size = args->block_size;

	for(size_t i = work_item_id*block_size; i < (work_item_id + 1)*block_size; i++) {
		out[work_item_id] += i;
	}
}



struct test_thread_args
{
	size_t buf_size;
	size_t block_size;
	size_t ntrials;
};

static int test_thread(void *ctx)
{
	struct test_thread_args *a = ctx;
	uint32_t *buf = malloc(sizeof(uint32_t)*a->buf_size);
	if(!buf) abort();
	struct work_fn_args args = { buf, a->block_size };
	for(size_t j=0; j<a->ntrials; j++) {
		paratask_call(paratask_default_instance(), 0, a->buf_size/a->block_size, work_function, &args);
		assert(buf[a->buf_size-1] == a->buf_size-1);
		for(size_t i = 0; i < a->buf_size; i++) {
			assert(buf[i] == i);
			buf[i] = 0;
		}
	}
	free(buf);
	return 0;
}

#define BUF_SIZE 32*1024*1024
#define BLOCK_SIZE 32

int main()
{
	struct paratask_ctx *paratask = paratask_new(16);

	uint32_t *buf1 = malloc(sizeof(uint32_t)*BUF_SIZE);
	struct work_fn_args args1 = { buf1, BLOCK_SIZE };
	paratask_call(paratask, 0, BUF_SIZE/BLOCK_SIZE, work_function, &args1);

	assert(buf1[BUF_SIZE-1] == BUF_SIZE-1);
	for(size_t i = 0; i < BUF_SIZE; i++) {
		assert(buf1[i] == i);
	}

	printf("Test1 Complete\n");

	uint32_t *buf2 = malloc(sizeof(uint32_t)*BUF_SIZE);
	struct work_fn_args args2 = { buf2, BLOCK_SIZE };

	memset(buf1, 0, sizeof(uint32_t)*BUF_SIZE);

	//struct paratask_task *paratask_call_async(struct paratask_ctx *self, size_t work_offset, size_t work_size, paratask_task_fn fn, void *arg);

	struct paratask_task *task1, *task2;

	task1 = paratask_call_async(paratask, 0, BUF_SIZE/BLOCK_SIZE, work_function, &args1);
	task2 = paratask_call_async(paratask, 0, BUF_SIZE/BLOCK_SIZE, work_function, &args2);

	paratask_wait(task1);
	paratask_wait(task2);
	paratask_delete(paratask);

	assert(buf1[BUF_SIZE-1] == BUF_SIZE-1);
	assert(buf2[BUF_SIZE-1] == BUF_SIZE-1);
	for(size_t i = 0; i < BUF_SIZE; i++) {
		assert(buf1[i] == i);
		assert(buf2[i] == i);
	}
	printf("Test2 Complete\n");

	free(buf1);
	free(buf2);

	size_t nthreads = 8;
	struct test_thread_args test_args = {1024, 8, 512*1024};
	thrd_t threads[nthreads];
	for(size_t i=0; i<nthreads; i++) {
		int err = thrd_create(&threads[i], test_thread, &test_args);
		if(err != thrd_success) abort();
	}

	for(size_t i=0; i<nthreads; i++) {
		thrd_join(threads[i], NULL);
	}

	printf("Test3 Complete\n");

	paratask = paratask_new(0);

	struct paratask_task *task_list[128];
	uint32_t *buf3 = malloc(sizeof(uint32_t)*128*1024*1024);
	struct work_fn_args args3 = { buf3, 128 }; 
	for(size_t i=0; i<128; i++) {
		task_list[i] = paratask_call_async(paratask, i*1024*1024, 1024*1024, work_function, &args1);
	}
	paratask_delete(paratask);
	unsigned int tasks_finished = 0;
	for(size_t i=0; i<128; i++) {
		if(!paratask_wait(task_list[i])) tasks_finished++;
	}

	printf("Test4 Complete, finished %ud\n", tasks_finished);

	return 0;
}
