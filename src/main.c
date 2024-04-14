#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "glb.h"

/* FGM (.fgm) is a file format which stand from "Fight Game Mesh". It is
 * a custom file format using data coming from GLTF (more accurately GLB)
 * to then format it to load faster.
 *
 * GLTF/GLB use JSON to represent 3D file content and in addition is very
 * slow to parse and has some useless information that adds up to its slow
 * speed. Hence why this file format was created to avoid those issues. It
 * converts GLB to FGM */

void mesh_info(struct BufferSizes sizes)
{
    printf("==BUFFER SIZES ==\nPosition: %d\nNormals: %d\nIndices: %d\nTexcoords: %d\n\n",
            sizes.position, sizes.normals, sizes.indices, sizes.texcoords);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("arguments must be path then name\n");
        return 1;
    }

    char *path = argv[1];
    char *fname = argv[2];

    int num_meshes = 0;
    struct BufferSizes *sizes = NULL;
    unsigned char *buffer = GLB_GetBufferData(&num_meshes, &sizes, path);

    if (buffer == NULL) {
        printf("Could not decode glb file aborting!\n");
        return 1;
    }

    FILE *fp = fopen(fname, "wb");
    for (int i =0; i < num_meshes; i++) {
        fwrite(&sizes[i].position, sizeof(uint64_t), 1, fp);
        fwrite(&sizes[i].normals, sizeof(uint64_t), 1, fp);
        fwrite(&sizes[i].texcoords, sizeof(uint64_t), 1, fp);
        fwrite(&sizes[i].indices, sizeof(uint64_t), 1, fp);

        printf("%d:\n", i);
        mesh_info(sizes[i]);
    }

    fwrite(buffer, buffer_position, 1, fp);

    fclose(fp);

    free(buffer);
    free(sizes);


    return 0;
}
