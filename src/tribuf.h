#ifndef TRIPLE_BUFFER_H
#define TRIPLE_BUFFER_H

/**
 * Garuntees:
 * tribuf_get_write() will never return the same buffer twice in a row (ie: you
 * can continue read from a buffer up until you do a second tribuf_finish_write()
 *
 * tribuf_get_read() will always return the most up to date buffer
 *
 * 
 */

typedef struct tribuf_s tribuf;

tribuf* tribuf_new(void **data, int locking);
void tribuf_destroy(tribuf *tb);

void* tribuf_get_write(tribuf *tb);
void* tribuf_get_read(tribuf *tb);
void* tribuf_get_read_nolock(tribuf *tb); ///< should only be used by write side to start up (if it needs a previous buf)
void tribuf_finish_read(tribuf *tb);
void tribuf_finish_write(tribuf *tb);
int tribuf_get_frmnum(tribuf *tb);

#endif
