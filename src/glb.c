#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>


#include "glb.h"
#include "gjson.h"

typedef struct
{
    unsigned char *data;
    uint32_t length;
} GLB_Chunk;

typedef struct
{
    int id;

    /* buffer info*/
    int byteLength;
    int byteOffset;

    /*accessor info*/
    int count;
    int data_size;
    int group_count;
} GLB_Attribute;

int buffer_position;

static char SIGN_BE[5] = {0x46, 0x54, 0x6C, 0x67}; /* big endian for ARM */
static char SIGN_LE[5] = {0x67, 0x6C, 0x54, 0x46}; /* little endian for x86 */

static void set_accessor(GLB_Attribute *attrib, gJSON *target)
{
    /* setting accessor values, 'componentType` will be used
     * to define data_size */

    attrib->count = gJSON_GetObjectItem(target, "count")->valueint;
    int componentType = gJSON_GetObjectItem(target, "componentType")->valueint;
    char *type = gJSON_GetObjectItem(target, "type")->valuestring;

    switch (componentType) {
        case 5125: /* unsigned int */
        case 5126: /* float */
            attrib->data_size = sizeof(float);
            break;
        case 5122: /* signed short */
        case 5123: /* unsigned short */
            attrib->data_size = sizeof(signed short);
            break;
        case 5121: /* unsigned byte */
        case 5120: /* signed byte */
        default:
            attrib->data_size = sizeof(unsigned char);
            break;
    }

    /* Defining the numbers in group */
    if (strncmp(type, "SCALAR", 6) == 0) {
        attrib->group_count = 1;
    } else if (strncmp(type,"VEC2", 4) == 0) {
        attrib->group_count = 2;
    } else if (strncmp(type, "VEC3", 4) == 0) {
        attrib->group_count = 3;
    } else if (strncmp(type, "VEC4", 4) == 0) {
        attrib->group_count = 4;
    } else if (strncmp(type, "MAT2", 4) == 0) {
        attrib->group_count = 4;
    } else if (strncmp(type, "MAT3", 4) == 0) {
        attrib->group_count = 9;
    } else if (strncmp(type, "MAT4", 4) == 0) {
        attrib->group_count = 16;
    } else {
        attrib->group_count = 1;
    }

    free(type);
}

static void set_bufferview(GLB_Attribute *bf, gJSON *source)
{
    bf->byteLength = gJSON_GetObjectItem(source, "byteLength")->valueint;
    bf->byteOffset = gJSON_GetObjectItem(source, "byteOffset")->valueint;
}

static void *read_buffer(GLB_Attribute mattr, unsigned char* bin_data)
{
    /* function that grabs copiesthe byte data then return a void pointer
     * that can be converted to the attribute data_type */

    /*unsigned char a_buffer[mattr.byteLength]; */
    unsigned char *a_buffer = malloc(mattr.byteLength);
    memcpy(a_buffer, bin_data+mattr.byteOffset, mattr.byteLength);

    int groupSize = mattr.group_count * mattr.data_size;
    void *buffer = malloc(mattr.data_size * mattr.group_count * mattr.count);

    void *dGroup = malloc(mattr.data_size);

    for (int i = 0; i < mattr.count; i++) {
        int index = i*groupSize;

        for (int j = 0; j < mattr.group_count; j++) {
            memcpy(dGroup, a_buffer + index + mattr.data_size*j, mattr.data_size);
            memcpy(buffer+index + mattr.data_size*j, dGroup, mattr.data_size);
        }

    }
    free(dGroup);

    return buffer;

}

static unsigned char *get_mesh(unsigned char *buffer, struct BufferSizes **sizes, int index, gJSON *gson, 
                               unsigned char* bin_data)
{
    /* process mesh attribute identified by index */

    gJSON *gmesh = gJSON_GetArrayItem(gJSON_GetObjectItem(gson, "meshes"), index);
    gJSON *gprim = gJSON_GetObjectItem(gmesh, "primitives")->child;
    gJSON *gattr = gJSON_GetObjectItem(gprim, "attributes");

    /* attributes */
    GLB_Attribute mesh_Position, mesh_Normal, mesh_Indices, mesh_Texcoord;

    mesh_Position.id = gJSON_GetObjectItem(gattr, "POSITION")->valueint;
    mesh_Indices.id = gJSON_GetObjectItem(gprim, "indices")->valueint;
    mesh_Normal.id = gJSON_GetObjectItem(gattr, "NORMAL")->valueint;
    mesh_Texcoord.id = gJSON_GetObjectItem(gattr, "TEXCOORD_0")->valueint;

    /* accessors, setting values */
    gJSON *access = gJSON_GetObjectItem(gson, "accessors");

    set_accessor(&mesh_Position, gJSON_GetArrayItem(access, mesh_Position.id));
    set_accessor(&mesh_Indices, gJSON_GetArrayItem(access, mesh_Indices.id));
    set_accessor(&mesh_Normal, gJSON_GetArrayItem(access, mesh_Normal.id));
    set_accessor(&mesh_Texcoord, gJSON_GetArrayItem(access, mesh_Texcoord.id));

    /* bufferviews */
    gJSON *bufferViews = gJSON_GetObjectItem(gson, "bufferViews");

    set_bufferview(&mesh_Position, gJSON_GetArrayItem(bufferViews, mesh_Position.id));
    set_bufferview(&mesh_Indices, gJSON_GetArrayItem(bufferViews, mesh_Indices.id));
    set_bufferview(&mesh_Normal, gJSON_GetArrayItem(bufferViews, mesh_Normal.id));
    set_bufferview(&mesh_Texcoord, gJSON_GetArrayItem(bufferViews, mesh_Texcoord.id));

    /* setting up buffer data*/
    float* vertices =           (float*)read_buffer(mesh_Position, bin_data);
    unsigned short* indices =   (unsigned short*)read_buffer(mesh_Indices, bin_data);
    float* normals =            (float*)read_buffer(mesh_Normal, bin_data);
    float* texcoords =         (float*)read_buffer(mesh_Texcoord, bin_data);
    int buffer_size = mesh_Position.byteLength + mesh_Normal.byteLength + mesh_Texcoord.byteLength + 
                        mesh_Indices.byteLength + buffer_position;

    buffer = realloc(buffer, buffer_size);

    /* has to be in this order check format file */
    memcpy(buffer + buffer_position, vertices, mesh_Position.byteLength);
    memcpy(buffer + buffer_position + mesh_Position.byteLength, normals, mesh_Normal.byteLength);

    memcpy(buffer + buffer_position + mesh_Position.byteLength + mesh_Normal.byteLength, texcoords, 
            mesh_Texcoord.byteLength);

    memcpy(buffer + buffer_position + mesh_Position.byteLength + mesh_Normal.byteLength + 
            mesh_Texcoord.byteLength, indices, mesh_Indices.byteLength);

    buffer_position = buffer_size;

    if (sizes == NULL)
        *sizes = malloc(sizeof(struct BufferSizes));
    else
        *sizes = realloc(*sizes, sizeof(struct BufferSizes)*(index+1));



    (*sizes)[index].position = mesh_Position.byteLength;
    (*sizes)[index].normals = mesh_Normal.byteLength;
    (*sizes)[index].texcoords = mesh_Texcoord.byteLength;
    (*sizes)[index].indices = mesh_Indices.byteLength;


    return buffer;
}

static unsigned char *get_mesh_from_gjson(int *num_meshes, struct BufferSizes **sizes, gJSON *gson, 
                                          unsigned char* bin_data)
{
    /*
    RNDR_Object *m_head = NULL;
    RNDR_Object *m_curr = NULL;
    */

    /* iterating through meshes */
    /*
    gJSON *gmeshes = gJSON_GetObjectItem(gson, "meshes");

    gJSON *g_curr = gmeshes->child;
    gJSON *g_next = NULL;

    if (g_curr == NULL) {
        printf("get_mesh_from_gjson : Error, no meshes!\n");
        goto fail;
    }

    int index = 0;
    do {
       
        if (m_head == NULL)
        {
            m_curr = m_head = get_mesh(index, gson, bin_data);
        } else{
            RNDR_Object *new = get_mesh(index, gson, bin_data);
            m_curr->next = new;
            m_curr->prev = m_curr;
            m_curr = new;
        }
    */
        /* iterating through every elements in
         * mesh */

    /*
        g_next = g_curr->next;
        g_curr = g_next;
        g_next = NULL;

        index++;
    
    } while (g_curr != NULL);


    gJSON_Delete(gmeshes);
    return m_head;
fail:
    gJSON_Delete(gmeshes);

    return NULL;
    */
    unsigned char *buffer = NULL;


    /* JSON setup */
    gJSON *gmeshes = gJSON_GetObjectItem(gson, "meshes");

    gJSON *g_curr = gmeshes->child;
    gJSON *g_next = NULL;
    buffer_position = 0;

    if (g_curr == NULL) {
        printf("get_mesh_from_gjson : Error, no meshes!\n");
        goto fail;
    }

    do {

        /*
        if (buffer == NULL)
        {
            buffer = get_mesh(num_meshes, gson, bin_data);
        } else{
            RNDR_Object *new = get_mesh(num_meshes, gson, bin_data);
            m_curr->next = new;
            m_curr->prev = m_curr;
            m_curr = new;
        }
        */

        buffer = get_mesh(buffer, sizes, *num_meshes, gson, bin_data);
        /* iterating through every elements in
         * mesh */

        g_next = g_curr->next;
        g_curr = g_next;
        g_next = NULL;

        *num_meshes = *num_meshes + 1;

    } while (g_curr != NULL);


    gJSON_Delete(gmeshes);
    return buffer;
fail:
    gJSON_Delete(gmeshes);
    return NULL;
}

static unsigned char *read_glb(int *num_meshes, struct BufferSizes **sizes, const char*path)
{
    /* there should only be 2 chunks in the GLB file,
     * JSON (mandatory) and BIN (chunk) optional but mandatory
     * for this decoder*/

    /* reading the header */

    FILE *fp;
    char sign[5];
    char log[215];
    uint32_t version, size; /* version must be 2 and size is the file size */

    if ((fp = fopen(path, "rb")) == NULL) {
        strcpy(log, "read_glb [Error] : could not open file");
        goto fail;
    }

    if (fread(sign, 1, 4, fp) != 4) {
        strcpy(log, "read_glb [Error] : signature length too hight");
        goto fail;
    }

    if (fread(&version, 4, 1, fp) != 1) {
        strcpy(log, "read_glb [Error] : version cannot be read");
        goto fail;
    }

    if (fread(&size, 4, 1, fp) != 1) {
        strcpy(log, "read_glb [ERROR] : could not read file size");
        goto fail;
    }

    if (version != 2 || (strncmp(sign, SIGN_BE, 4) != 0 &&
        strncmp(sign, SIGN_LE, 4)) != 0) {
        strcpy(log, "read_glb [Error] : open failed, possible reason version != 2, bad signature");
        goto fail;
    }

    /* reading chunks JSON is the first chunk */
    GLB_Chunk json_chunk, bin_chunk;

    char chunk_type[5];
    chunk_type[4] = '\0';

    if (fread(&json_chunk.length, 4, 1, fp) != 1) {
        printf("read_chunk : could not read json chunk\n");
        goto fail;
    }

    fread(chunk_type, 4, 1, fp); /* skipping type */
    json_chunk.data = malloc(sizeof(unsigned char)*json_chunk.length);
    fread(json_chunk.data, json_chunk.length, 1, fp);

    if (fread(&bin_chunk.length, 4, 1, fp) != 1) {
        printf("read_chunk : could not read bin chunk\n");
        goto fail;
    }

    fread(chunk_type, 4, 1, fp); /* skipping type */
    bin_chunk.data = malloc(sizeof(unsigned char)*bin_chunk.length);
    fread(bin_chunk.data, bin_chunk.length, 1, fp);

    fclose(fp);

    /* decoding */
    gJSON *gson = gJSON_ParseWithLength(json_chunk.data, json_chunk.length);
    unsigned char *mesh = get_mesh_from_gjson(num_meshes, sizes, gson, bin_chunk.data);
    gJSON_Delete(gson);

    free(json_chunk.data);
    free(bin_chunk.data);


    return mesh;

fail:

    printf("%s\n", log);
    return NULL;
}


unsigned char *GLB_GetBufferData(int *num_meshes, struct BufferSizes **sizes, const char *path)
{
    return read_glb(num_meshes, sizes, path);
}
