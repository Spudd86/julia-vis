#ifndef MAXSRC_H_
#define MAXSRC_H_

struct maxsrc;
struct maxsrc *maxsrc_new(int w, int h);
void maxsrc_delete(struct maxsrc *self);
const uint16_t *maxsrc_get(struct maxsrc *self);
void maxsrc_update(struct maxsrc *self, const float *audio, int audiolen);


#endif
