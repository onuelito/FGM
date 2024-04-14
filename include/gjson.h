#ifndef __GJSON_DECODER__
#define __GJSON_DECODER__

/* data types according to JSON specifications */
enum gJSON_type
{
    gJSON_Null,
    gJSON_Object,
	gJSON_Number,
    gJSON_Array,
    gJSON_String,
    gJSON_True,
    gJSON_False,
};

typedef struct gJSON
{
    /* for array and objects */
    struct gJSON *next;
    struct gJSON *prev;
    struct gJSON *child;

    int type;
    char *valuestring;
    double valuedouble;
    int valueint;

    char *string; /* keys for objects */
} gJSON;

void gJSON_Delete(gJSON*);

gJSON *gJSON_GetArrayItem(gJSON*, int);
gJSON *gJSON_GetObjectItem(gJSON*, const char*);
gJSON *gJSON_ParseWithLength(unsigned char*, size_t);

#endif
