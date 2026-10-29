#ifndef GFX_H
#define GFX_H

#define RGBA(r,g,b,a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define RGB(r,g,b) RGBA(r,g,b,255)

void gfx_init(void);
void gfx_begin(unsigned clear_color);
void gfx_end(void);

/* 3D */
void gfx_3d(void);
void gfx_camera(float ex, float ey, float ez, float cx, float cy, float cz);
void gfx_box(float x, float y, float z, float w, float h, float d, unsigned color);
void gfx_box_rot(float x, float y, float z, float w, float h, float d, float yaw, unsigned color);

/* 2D overlay (screen pixels, 480x272) */
void gfx_2d(void);
void gfx_rect(float x, float y, float w, float h, unsigned color);
void gfx_gradient(float x, float y, float w, float h, unsigned top, unsigned bottom);
void gfx_text(float x, float y, int scale, unsigned color, const char *s);
int  gfx_text_wrap(float x, float y, int scale, unsigned color, const char *s, int maxchars);
int  gfx_text_width(const char *s, int scale);

#endif
