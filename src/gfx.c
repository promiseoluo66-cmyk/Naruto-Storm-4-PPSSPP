/*
 * gfx.c - minimal PSP GU renderer: 3D boxes (vertical gradient shading), 2D rects,
 * gradients, built-in 5x7 bitmap font with drop shadow.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>
#include <math.h>
#include "gfx.h"

#define BUF_W 512
#define SCR_W 480
#define SCR_H 272

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct { unsigned int color; float x, y, z; } Vtx;

void gfx_init(void)
{
    void *fbp0 = (void *)0;
    void *fbp1 = (void *)(BUF_W * SCR_H * 4);
    void *zbp  = (void *)(BUF_W * SCR_H * 4 * 2);

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, fbp1, BUF_W);
    sceGuDepthBuffer(zbp, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void gfx_begin(unsigned clear_color)
{
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(clear_color);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void gfx_3d(void)
{
    sceGuEnable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(60.0f, (float)SCR_W / (float)SCR_H, 0.5f, 140.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void gfx_camera(float ex, float ey, float ez, float cx, float cy, float cz)
{
    ScePspFVector3 eye = { ex, ey, ez };
    ScePspFVector3 ctr = { cx, cy, cz };
    ScePspFVector3 up  = { 0.0f, 1.0f, 0.0f };
    ScePspFMatrix4 m;
    gumLoadIdentity(&m);
    gumLookAt(&m, &eye, &ctr, &up);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadMatrix(&m);
}

static unsigned shade(unsigned c, float f)
{
    unsigned r = (unsigned)((c & 255) * f);
    unsigned g = (unsigned)(((c >> 8) & 255) * f);
    unsigned b = (unsigned)(((c >> 16) & 255) * f);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

void gfx_box_rot(float x, float y, float z, float w, float h, float d, float yaw, unsigned color)
{
    static const int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };
    static const float sh[6] = { 0.74f, 0.88f, 0.62f, 0.62f, 1.10f, 0.45f };
    float hx = w * 0.5f, hy = h * 0.5f, hz = d * 0.5f;
    float c[8][3] = {
        {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {-hx, hy, -hz},
        {-hx, -hy,  hz}, {hx, -hy,  hz}, {hx, hy,  hz}, {-hx, hy,  hz}
    };
    Vtx *v = (Vtx *)sceGuGetMemory(36 * sizeof(Vtx));
    int f, i = 0;
    ScePspFVector3 t = { x, y, z };

    for (f = 0; f < 6; f++) {
        static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
        int k;
        for (k = 0; k < 6; k++) {
            int ci = faces[f][tri[k]];
            /* darker towards the bottom of side faces: soft vertical gradient */
            float g = (f < 4 && c[ci][1] < 0.0f) ? 0.72f : 1.0f;
            v[i].color = shade(color, sh[f] * g);
            v[i].x = c[ci][0];
            v[i].y = c[ci][1];
            v[i].z = c[ci][2];
            i++;
        }
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumTranslate(&t);
    if (yaw != 0.0f) sceGumRotateY(yaw);
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 36, 0, v);
}


/* Rounded low-poly ellipsoid for more natural anime-style character silhouettes. */
void gfx_ellipsoid(float x, float y, float z, float w, float h, float d, unsigned color)
{
    enum { LAT = 6, LON = 10 };
    Vtx *v = (Vtx *)sceGuGetMemory(LAT * LON * 6 * sizeof(Vtx));
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    const float pi = 3.14159265358979323846f;
    ScePspFVector3 t = { x, y, z };
    int lat, lon, n = 0;
    for (lat = 0; lat < LAT; lat++) {
        float a0 = -pi * 0.5f + pi * (float)lat / LAT;
        float a1 = -pi * 0.5f + pi * (float)(lat + 1) / LAT;
        for (lon = 0; lon < LON; lon++) {
            float b0 = 2.0f * pi * (float)lon / LON;
            float b1 = 2.0f * pi * (float)(lon + 1) / LON;
            float px[4] = { cosf(a0)*cosf(b0)*w*0.5f, cosf(a0)*cosf(b1)*w*0.5f,
                            cosf(a1)*cosf(b1)*w*0.5f, cosf(a1)*cosf(b0)*w*0.5f };
            float py[4] = { sinf(a0)*h*0.5f, sinf(a0)*h*0.5f,
                            sinf(a1)*h*0.5f, sinf(a1)*h*0.5f };
            float pz[4] = { cosf(a0)*sinf(b0)*d*0.5f, cosf(a0)*sinf(b1)*d*0.5f,
                            cosf(a1)*sinf(b1)*d*0.5f, cosf(a1)*sinf(b0)*d*0.5f };
            int k;
            for (k = 0; k < 6; k++) {
                int q = tri[k];
                float light = 0.76f + 0.24f * ((py[q] / (h * 0.5f + 0.001f) + 1.0f) * 0.5f);
                v[n].color = shade(color, light);
                v[n].x = px[q]; v[n].y = py[q]; v[n].z = pz[q]; n++;
            }
        }
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumTranslate(&t);
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, v);
}

void gfx_box(float x, float y, float z, float w, float h, float d, unsigned color)
{
    gfx_box_rot(x, y, z, w, h, d, 0.0f, color);
}

void gfx_2d(void)
{
    sceGuDisable(GU_DEPTH_TEST);
}

void gfx_rect(float x, float y, float w, float h, unsigned color)
{
    Vtx *v = (Vtx *)sceGuGetMemory(2 * sizeof(Vtx));
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

void gfx_gradient(float x, float y, float w, float h, unsigned top, unsigned bottom)
{
    Vtx *v = (Vtx *)sceGuGetMemory(4 * sizeof(Vtx));
    v[0].color = top;    v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = top;    v[1].x = x + w; v[1].y = y;     v[1].z = 0.0f;
    v[2].color = bottom; v[2].x = x;     v[2].y = y + h; v[2].z = 0.0f;
    v[3].color = bottom; v[3].x = x + w; v[3].y = y + h; v[3].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, v);
}

/* 5x7 font, 5 bits per row, bit 4 = leftmost pixel */
static const char GCH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:.!-/>,'()<";
static const unsigned char GLY[][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x00,0x04,0x00,0x00,0x00,0x04,0x00}, /* : */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* ! */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10}, /* / */
    {0x10,0x08,0x04,0x02,0x04,0x08,0x10}, /* > */
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}, /* , */
    {0x04,0x04,0x08,0x00,0x00,0x00,0x00}, /* ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ) */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}  /* < */
};

static const unsigned char *glyph_for(char c)
{
    const char *p;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == 0 || c == ' ') return NULL;
    p = strchr(GCH, c);
    if (!p) return NULL;
    return GLY[p - GCH];
}

int gfx_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 6 * scale;
}

static void text_raw(float x, float y, int scale, unsigned color, const char *s)
{
    int count = 0, n = (int)strlen(s), i, r, c;
    Vtx *v;
    int vi = 0;
    float sc = (float)scale;

    for (i = 0; i < n; i++) {
        const unsigned char *g = glyph_for(s[i]);
        if (!g) continue;
        for (r = 0; r < 7; r++)
            for (c = 0; c < 5; c++)
                if (g[r] & (0x10 >> c)) count++;
    }
    if (count == 0) return;

    v = (Vtx *)sceGuGetMemory(count * 2 * sizeof(Vtx));
    for (i = 0; i < n; i++) {
        const unsigned char *g = glyph_for(s[i]);
        float ox = x + i * 6 * sc;
        if (!g) continue;
        for (r = 0; r < 7; r++)
            for (c = 0; c < 5; c++)
                if (g[r] & (0x10 >> c)) {
                    float px = ox + c * sc, py = y + r * sc;
                    v[vi].color = color; v[vi].x = px;      v[vi].y = py;      v[vi].z = 0.0f; vi++;
                    v[vi].color = color; v[vi].x = px + sc; v[vi].y = py + sc; v[vi].z = 0.0f; vi++;
                }
    }
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, vi, 0, v);
}

/* text with a dark drop shadow so it stays readable on bright backgrounds */
void gfx_text(float x, float y, int scale, unsigned color, const char *s)
{
    float off = (scale >= 2) ? 2.0f : 1.0f;
    text_raw(x + off, y + off, scale, RGBA(0, 0, 0, 210), s);
    text_raw(x, y, scale, color, s);
}

int gfx_text_wrap(float x, float y, int scale, unsigned color, const char *s, int maxchars)
{
    char line[80];
    const char *p = s;
    int lines = 0;

    if (maxchars > 78) maxchars = 78;
    while (*p) {
        int len = 0, last = -1;
        while (p[len] && len < maxchars) {
            if (p[len] == ' ') last = len;
            len++;
        }
        if (p[len] && p[len] != ' ' && last > 0) len = last;
        memcpy(line, p, (size_t)len);
        line[len] = 0;
        gfx_text(x, y + lines * (8 * scale + 2), scale, color, line);
        lines++;
        p += len;
        while (*p == ' ') p++;
    }
    return lines;
}
