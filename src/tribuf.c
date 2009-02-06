
#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#include "tribuf.h"
#include "common.h"

struct tribuf_s {
	unsigned int frame;
	unsigned int frms[3];
	int lastmin;
	void **data;
};

// does this really need a mutex or would a storage fence in write be enough? (do we even need that?)

tribuf* tribuf_new(void **data)
{
	tribuf *tb = xmalloc(sizeof(tribuf));
	
	tb->data = data;
	tb->frame = tb->lastmin = 0;
	for(int i=0; i<3; i++) tb->frms[i] = i;
	return tb;
}

void tribuf_destroy(tribuf *tb)
{
	free(tb);
}

void* tribuf_get_write(tribuf *tb)
{
	int min = (tb->frms[1] < tb->frms[0])? 1 : 0;
	min = (tb->frms[2] < tb->frms[min])? 2 : min;
	tb->lastmin = min;
	
	return tb->data[min];
}

void tribuf_finish_write(tribuf *tb)
{
	tb->frms[tb->lastmin] = tb->frame;
	tb->frame++;
}

void* tribuf_get_read(tribuf *tb)
{
	int max = (tb->frms[1] > tb->frms[0])? 1 : 0;
	max = (tb->frms[2] > tb->frms[max])? 2 : max;
	return tb->data[max];
}

int tribuf_get_frmnum(tribuf *tb) {
	return tb->frame;
}
