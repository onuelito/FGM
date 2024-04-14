/* Decoder for Khronos Group's 3d model file format
 * GTF2.0. More accurately .glb files */

#ifndef __DECODER_GLB__
#define __DECODER_GLB__

#include <stdint.h>

struct BufferSizes {
    uint64_t position;
    uint64_t normals;
    uint64_t texcoords;
    uint64_t indices;
};

extern int buffer_position;

unsigned char *GLB_GetBufferData(uint16_t *num_meshes, struct BufferSizes**, const char *);

#endif

