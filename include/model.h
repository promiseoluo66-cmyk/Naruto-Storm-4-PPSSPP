#ifndef MODEL_H
#define MODEL_H

/*
 * NMDL v2 mesh format (little-endian):
 *   "NMDL", u32 version=2, u32 vertex_count
 *   tightly-packed 24-byte records: f32 u,v; u32 GU RGBA8888 color;
 *   f32 x,y,z. Legacy v1 files (color,x,y,z; 16 bytes) remain readable.
 *
 * NMTX v1 texture format:
 *   "NMTX", u32 version=1, u32 width, u32 height, u32 format=3 (RGBA8888),
 *   followed by width*height RGBA8888 pixels.
 * Texture dimensions must be powers of two between 8 and 512.
 *
 * ModelVertex deliberately has a 24-byte stride. Align the allocated buffer
 * to 16 bytes (done by model_load); do not apply aligned(16) to this record,
 * because that would pad sizeof(ModelVertex) to 32 and break GU vertex fetch.
 */
typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} ModelVertex;

typedef struct {
    ModelVertex *vertices;
    unsigned int vertex_count;
} Model;

typedef struct {
    void *pixels;
    unsigned int width;
    unsigned int height;
} ModelTexture;

int model_load(Model *model, const char *path);
int model_texture_load(ModelTexture *texture, const char *path);
void model_texture_bind(const ModelTexture *texture);
void model_texture_unbind(void);
void model_draw(const Model *model, float x, float y, float z, float yaw);
void model_draw_textured(const Model *model, const ModelTexture *texture,
                         float x, float y, float z, float yaw);
void model_free(Model *model);
void model_texture_free(ModelTexture *texture);

#endif
