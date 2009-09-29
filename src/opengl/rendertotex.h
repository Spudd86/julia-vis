/**
 * rendertotex.h
 *
 */

#ifndef RENDERTOTEX_H_
#define RENDERTOTEX_H_

typedef struct _TexRenderContext_s TexRenderContext;

/* bind the last rendered frame texture into the state on the given target
 */
void tex_render_bind_tex(TexRenderContext *self, GLenum target);

void tex_render_set_clamp(TexRenderContext *self, GLint clamp_mode);

void tex_render_begin(TexRenderContext *self);
void tex_render_end(TexRenderContext *self);


#endif /* include guard */
