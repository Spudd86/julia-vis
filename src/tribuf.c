
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#include <SDL_thread.h>

#include "tribuf.h"

struct tribuf_s {
	SDL_mutex *mutex;
	unsigned int frame;
	unsigned int frms[3];
	int lastmin;
	void **data;
};

tribuf* tribuf_new(void **data)
{
	tribuf *tb = malloc(sizeof(tribuf));
	if(tb==NULL) abort();
	
	tb->mutex = SDL_CreateMutex();
	tb->data = data;
	tb->frame = tb->lastmin = 0;
	for(int i=0; i<3; i++) tb->frms[i] = i;
	return tb;
}

void tribuf_destroy(tribuf *tb)
{
	SDL_DestroyMutex(tb->mutex);
	free(tb);
}

void* tribuf_get_write(tribuf *tb)
{	
	//SDL_LockMutex(tb->mutex);
	
	int min = (tb->frms[1] < tb->frms[0])? 1 : 0;
	min = (tb->frms[2] < tb->frms[min])? 2 : min;
	tb->lastmin = min;
	//printf("next buf: %i framenum: %i\n", min, tb->frms[min]);
	//SDL_UnlockMutex(tb->mutex);
	
	return tb->data[min];
}

void tribuf_finish_write(tribuf *tb)
{
	SDL_LockMutex(tb->mutex);
	tb->frms[tb->lastmin] = tb->frame;
	tb->frame++;
	SDL_UnlockMutex(tb->mutex);
}

void* tribuf_get_read(tribuf *tb)
{
	SDL_LockMutex(tb->mutex);
	int max = (tb->frms[1] > tb->frms[0])? 1 : 0;
	max = (tb->frms[2] > tb->frms[max])? 2 : max;
	//printf("next disp: %i framenum: %i\n", max, tb->frms[max]);
	SDL_UnlockMutex(tb->mutex);
	return tb->data[max];
}