#ifndef GLSCOPE_H
#define GLSCOPE_H
struct glscope_ctx;

struct glscope_ctx *gl_scope_init(int width, int height, int num_samp, GLboolean force_fixed);
void render_scope(struct glscope_ctx *ctx, float R[3][3], const float *data, int len);
#endif

