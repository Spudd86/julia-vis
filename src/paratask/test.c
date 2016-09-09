#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>

#include "paratask.h"

struct work_fn_args
{
	uint32_t *out;
	size_t block_size;
};

void work_function(size_t work_item_id, void *args_)
{
	struct work_fn_args *args = args_;
	uint32_t *out = args->out;
	size_t block_size = args->block_size;

	for(size_t i = work_item_id*block_size; i < (work_item_id + 1)*block_size; i++) {
		out[i] = i;
	}
}

#define BUF_SIZE 512*512
#define BLOCK_SIZE 64

int main()
{
	struct paratask_ctx *paratask = paratask_new(2);

	uint32_t *buf = malloc(sizeof(uint32_t)*BUF_SIZE);
	struct work_fn_args args = { buf, BLOCK_SIZE };
	paratask_call(paratask, 0, BUF_SIZE/BLOCK_SIZE, work_function, &args);

	for(size_t i = 0; i < BUF_SIZE; i++) {
		assert(buf[i] == i);
	}
}