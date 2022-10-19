#ifndef SCOPE_RENDER_H_
#define SCOPE_RENDER_H_

#include "common.h"

struct scope_renderer;

struct scope_renderer* scope_renderer_new(int w, int h, int samp);
void scope_renderer_delete(struct scope_renderer*);
void scope_render(struct scope_renderer *self,
                  void *restrict dest,
                  float tx, float ty, float tz,
                  const float *audio, int audiolen);



#endif
