#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#ifdef TRIBUF_LOCKING
//#define TB_LOCK_PROFILE

#include <sys/types.h>
#include <pthread.h>
#include <error.h>
#include <errno.h>
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
#define tb_mutex_destroy pthread_mutex_destroy
#define tb_mutex_init(m) pthread_mutex_init((m), NULL)
#endif


#include "tribuf.h"
#include "common.h"

struct tribuf_s {
	void **data;
#ifdef TRIBUF_LOCKING
	tb_mutex locks[3];
	int lastread;
	int dolock;
#endif
	unsigned int frms[3];
	unsigned int lastmin;
	unsigned int frame;
	unsigned int next_buf;
};

#ifdef TB_LOCK_PROFILE
	static unsigned int lock_frms = 0;
	static unsigned int lock_contend = 0;
	static unsigned int lock_trys = 0;
	static unsigned int lock_mins = 0;
	static unsigned int lock_alts = 0;
	static void  tb_lock_prof_print(void) {
		printf("tribuf lock stats:\n\tfrms: %i\n\tcont: %i\n\ttrys: %i\n\tmins: %i\n\talts: %i\n", lock_frms, lock_contend, lock_trys, lock_mins, lock_alts);
		printf("\tpercent contended: %i\n", (lock_contend*100)/lock_trys);
	}
	static void __attribute__((constructor)) tb_lock_prof_init(void) {
		atexit(tb_lock_prof_print);
	}
#endif

// TODO: reader/writer locks rather than mutex's?

tribuf* tribuf_new(void **data, int locking)
{
	tribuf *tb = xmalloc(sizeof(tribuf));

	tb->data = data;
	tb->frame = tb->lastmin = 0;
	tb->next_buf = 2;

	for(int i=0; i<3; i++) tb->frms[i] = i;

#ifdef TRIBUF_LOCKING
	tb->dolock = locking;
	tb->lastread = -1;
	if(tb->dolock) for(int i=0; i<3; i++) tb_mutex_init(&tb->locks[i]);
#else
	(void)locking;
#endif
	return tb;
}

void tribuf_destroy(tribuf *tb)
{
#ifdef TRIBUF_LOCKING
	if(tb->dolock) for(int i=0; i<3; i++) tb_mutex_destroy(&tb->locks[i]);
#endif
	free(tb);
}

void* tribuf_get_write(tribuf *tb)
{
#ifdef TRIBUF_LOCKING
	if(tb->dolock) {
		//~ tb_lock(&tb->locks[tb->lastmin]);
		int res = tb->lastmin;
		int a = (res+1)%3, b =  (res+2)%3;
		int alt = (tb->frms[a] < tb->frms[b])? a : b;
	#ifndef TB_LOCK_PROFILE
		while(1) {
			if(tb_trylock(&tb->locks[res])) break;
			int tmp = alt;
			alt = res;
			res = tmp;
		}
	#else
		__sync_add_and_fetch(&lock_frms, 1);
		__sync_add_and_fetch(&lock_trys, 1);
		if(tb_trylock(&tb->locks[tb->lastmin])) { __sync_add_and_fetch(&lock_mins, 1); return tb->data[tb->lastmin]; }
		__sync_add_and_fetch(&lock_contend, 1);
		while(1) {
			res = alt; __sync_add_and_fetch(&lock_trys, 1);
			if(tb_trylock(&tb->locks[res])) {  __sync_add_and_fetch(&lock_alts, 1); break;}
			res = tb->lastmin; __sync_add_and_fetch(&lock_trys, 1);
			if(tb_trylock(&tb->locks[res])) { __sync_add_and_fetch(&lock_mins, 1); break; }
		}
	#endif
		tb->lastmin = res;
	}
#endif
	return tb->data[tb->lastmin];
}

void tribuf_finish_write(tribuf *tb)
{
#ifdef TRIBUF_LOCKING
	if(tb->dolock)  tb_unlock(&tb->locks[tb->lastmin]);
#endif
	tb->next_buf = tb->lastmin;
	//tb->frms[tb->lastmin] = __sync_add_and_fetch(&tb->frame, 1);
	tb->frms[tb->lastmin] = ++tb->frame;

	int a = (tb->lastmin+1)%3, b =  (tb->lastmin+2)%3;
	int min = (tb->frms[a] < tb->frms[b])? a : b;
	tb->lastmin = min;
}

void* tribuf_get_read(tribuf *tb)
{
	//int bufnum = __sync_add_and_fetch(&tb->next_buf, 0);
	int bufnum = tb->next_buf;
#ifdef TRIBUF_LOCKING
	if(tb->dolock) tb_lock(&tb->locks[bufnum]);
	tb->lastread = bufnum;
#endif
	return tb->data[bufnum];
}

void* tribuf_get_read_nolock(tribuf *tb)
{
	int bufnum = __sync_add_and_fetch(&tb->next_buf, 0);
	return tb->data[bufnum];
}

void tribuf_finish_read(tribuf *tb)
{
#ifdef TRIBUF_LOCKING
	if(tb->lastread != -1 && tb->dolock)
		tb_unlock(&tb->locks[tb->lastread]);
#else
	(void)tb;
#endif
}

int tribuf_get_frmnum(tribuf *tb) {
	return __sync_add_and_fetch(&tb->frame, 0);
}
