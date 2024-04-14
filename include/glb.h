/* Decoder for Khronos Group's 3d model file format
 * GTF2.0. More accurately .glb files */

#ifndef __DECODER_GLB__
#define __DECODER_GLB__

struct BufferSizes {
    int position;
    int normals;
    int texcoords;
    int indices;
};

extern int buffer_position;

unsigned char *GLB_GetBufferData(int *num_meshes, struct BufferSizes**, const char *);

#endif

