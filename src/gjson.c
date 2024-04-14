#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#include "gjson.h"

#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

#define can_read(buffer, size) ((buffer != NULL) && ((buffer)->offset + size) <= (buffer)->length)
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))

#define gJSON_IsReference 255
#define GJSON_NESTING_LIMIT 100

typedef struct
{
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth;
} parse_buffer;

static bool parse_string(gJSON *item, parse_buffer *buffer);
static bool parse_object(gJSON *item, parse_buffer *buffer);
static bool parse_array(gJSON *item, parse_buffer *buffer);

static unsigned char get_decimal_point(void)
{
#ifdef ENABLE_LOCALES
    struct lconv *lconv = localeconv();
    return (unsigned char) lconv->decimal_point[0];
#else
    return '.';
#endif
}

static int case_insensitive_strcmp(const unsigned char* string1, const unsigned char* string2)
{
    if ((string1 == NULL) || (string2 == NULL))
        return 1;

    if (string1 == string2)
        return 0;

    for (; tolower(*string1) == tolower(*string2); (void)string1++, (void)string2++)
        if (*string1 == '\0')
            return 0;

    return tolower(*string1) - tolower(*string2);
}

static gJSON *get_object_item(gJSON *object, const char *name, const bool case_sensitive)
{
    gJSON *current_element = NULL;

    if ((object == NULL) || (name == NULL))
        return NULL;

    current_element = object->child;
    if (case_sensitive) {
        while ((current_element != NULL) && (current_element->string != NULL) && (strcmp(name, current_element->string) != 0)) {
                current_element = current_element->next;
        }
    } else {
       while ((current_element != NULL) && (case_insensitive_strcmp((const unsigned char*)name, (const unsigned char*)(current_element->string)) != 0)) {
                                 current_element = current_element->next;
        } 
    }

    return current_element;
} 

static gJSON *get_array_item(gJSON *array, size_t index)
{
    gJSON *current_child = NULL;

    if (array == NULL)
        return NULL;

    current_child = array->child;
    while ((current_child != NULL) && (index > 0)) {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

gJSON *gJSON_GetObjectItem(gJSON *object, const char *string)
{
    return get_object_item(object, string, false);
}

gJSON *gJSON_GetArrayItem(gJSON *array, int index)
{
    if (index < 0)
        return NULL;

    return get_array_item(array, (size_t) index);
}

static gJSON *gJSON_New_Item(void)
{
    gJSON *node = (gJSON*) malloc(sizeof(gJSON));
    if (node) {
        memset(node, '\0', sizeof(gJSON));
    }

    return node;
}

void gJSON_Delete(gJSON *item)
{
    gJSON *next = NULL;
    while (item != NULL)
    {
        next = item->next;

        if (!(item->type & gJSON_IsReference) && (item->child != NULL)) {
            gJSON_Delete(item->child);
        }

        free(item);
        item = next;
    }
}

bool parse_string(gJSON *item, parse_buffer *buffer)
{
    const unsigned char *input_pointer = buffer_at_offset(buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;

    if (buffer_at_offset(buffer)[0] != '\"')
        goto fail;

    {
        /* Calculate size of output*/
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;

        while (((size_t)(input_end - buffer->content) < buffer->length) && (*input_end != '\"')) {
            
            if (input_end[0] == '\\') {
                if ((size_t)(input_end + 1 - buffer->content) >= buffer->length)
                    goto fail;

                skipped_bytes++;
                input_end++;

            }

            input_end++;

        }
        
        if (((size_t)(input_end - buffer->content) >= buffer->length) || (*input_end != '\"'))
            goto fail;

        allocation_length = (size_t) (input_end - buffer_at_offset(buffer)) - skipped_bytes;
        output = (unsigned char*) malloc(allocation_length + sizeof(""));

        if (output == NULL)
            goto fail;

    }

    output_pointer = output;

    while (input_pointer < input_end) {
        *output_pointer++ = *input_pointer++;
    }

    *output_pointer = '\0';

    item->type = gJSON_String;
    item->valuestring = (char*) output;

    buffer->offset = (size_t) (input_end - buffer->content);
    buffer->offset++;
    

    return true;

fail:
   	printf("NOT A STRING\n");

    return false;
}

static bool parse_number(gJSON *item, parse_buffer *buffer)
{
    double number = 0;
    unsigned char *after_end = NULL;
    unsigned char number_c_string[64];
    unsigned char decimal_point = get_decimal_point();
    size_t i = 0;

    if ((buffer == NULL) || (buffer->content == NULL))
        return false;


    /* Copy to temporary buffer and replace '.' by current locale decimal
     * point */

    for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(buffer, i); i++) {
        switch (buffer_at_offset(buffer)[i]) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '+':
            case '-':
            case 'e':
            case 'E':
                number_c_string[i] = buffer_at_offset(buffer)[i];
                break;

            case '.':
                number_c_string[i] = decimal_point;
                break;

            default:
                goto loop_end;

        }
    }


loop_end:
    number_c_string[i] = '\0';

    number = strtod((const char*)number_c_string, (char**)&after_end);
    if (number_c_string == after_end)
        return false;

    item->valuedouble = number;

    if (number > INT_MAX)
        item->valueint = INT_MAX;
    else if (number <= (double)INT_MIN)
        item->valueint = INT_MIN;
    else
        item->valueint = (int)number;

    item->type = gJSON_Number;

    buffer->offset += (size_t)(after_end - number_c_string);
    return true;
}


/* data creation function */
static bool parse_value(gJSON* item, parse_buffer *buffer)
{
    if ((buffer == NULL) || (buffer->content) == NULL)
        return false;

    /* null */
    if (can_read(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "null", 4) == 0)) {
        item->type = gJSON_Null;
        buffer->offset += 4;
        return true;
    }

    /* false */
    if (can_read(buffer, 5) && (strncmp((const char*)buffer_at_offset(buffer), "false", 5) == 0)) {
        item->type = gJSON_False;
        buffer->offset += 5;
        return true;
    }

    /* true */
    if (can_read(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "true", 4) == 0)) {
        item->type = gJSON_True;
        buffer->offset += 4;
        return true;
    }

    /* string */
    if (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == '\"')) {
        return parse_string(item, buffer);
    }

    /* number */
    if (can_access_at_index(buffer, 0) && ((buffer_at_offset(buffer)[0] == '-') || ((buffer_at_offset(buffer)[0] >= '0') && (buffer_at_offset(buffer)[0] <= '9')))) {
        return parse_number(item, buffer);
    }

    /* array */
    if (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == '[')) {
        return parse_array(item, buffer);
    }

    /* object */
    if (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == '{'))
    {
        return parse_object(item, buffer);
    }

    return false;
}

static bool parse_array(gJSON *item, parse_buffer *buffer)
{
    gJSON *head = NULL;
    gJSON *current_item = NULL;

    if (buffer->depth >= GJSON_NESTING_LIMIT)
        return false;

    buffer->depth++;

    if (buffer_at_offset(buffer)[0] != '[')
        goto fail;

    buffer->offset++;
    if (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == ']'))
        goto success; /* Empty array */

    if (cannot_access_at_index(buffer, 0)) {
        buffer->offset--;
        goto fail;
    }

    buffer->offset--;
    do
    {
        gJSON *new_item = gJSON_New_Item();

        if (new_item == NULL)
            goto fail;

        if (head == NULL)
            current_item = head = new_item;
        else {
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        buffer->offset++;

        if (parse_value(current_item, buffer) == false)
            goto fail;

    } while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == ','));

    if (cannot_access_at_index(buffer, 0) || buffer_at_offset(buffer)[0] != ']')
        goto fail; /* end of array expected */

success:
    buffer->depth--;

    if (head != NULL)
        head->prev = current_item;

    item->type = gJSON_Array;;
    item->child = head;

    buffer->offset++;

    return true;

fail:

    printf("parse_array : failed\n");
    return false;
}

static bool parse_object(gJSON *item, parse_buffer *buffer)
{
    gJSON *head = NULL;
    gJSON *current_item = NULL;

    if (buffer->depth >= GJSON_NESTING_LIMIT)
        goto fail;

    buffer->depth++;

    if (cannot_access_at_index(buffer, 0) || (buffer_at_offset(buffer)[0]) != '{') {
        goto fail;
    }

    buffer->offset++;

    if (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0]) == '}') {
        /* empty object */
        goto success;
    }

    /* did we go pass length? */
    if (cannot_access_at_index(buffer, 0)) {
        buffer->offset--;
        goto fail;
    }


    buffer->offset--;
    do
    {
        gJSON *new_item = gJSON_New_Item();
        if (new_item == NULL)
            goto fail;

        if (head == NULL)
            current_item = head = new_item;
        else {
            /* add to end */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        buffer->offset++;
        if (parse_string(current_item, buffer) == false) {
            goto fail;
        }

        /* swap value and string cause name was parsed */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (cannot_access_at_index(buffer, 0) || (buffer_at_offset(buffer)[0] != ':'))
            goto fail;
        buffer->offset++;

        if (parse_value(current_item, buffer) == false)
            goto fail;


    }
    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] == ','));


    if (cannot_access_at_index(buffer, 0) || (buffer_at_offset(buffer)[0] != '}')) {
        goto fail; /* expected object end */
    }


success:
    buffer->depth--;

    if (head != NULL)
        head->prev = current_item;

    item->type = gJSON_Object;
    item->child = head;

    buffer->offset++;
    return true;

fail:
    if (buffer->depth >= GJSON_NESTING_LIMIT) {
        printf("parse_object : nesting limit exceeded %d. Aborting\n", GJSON_NESTING_LIMIT);
    }
    printf("parse_object : failed\n");

    return false;
}

/* Creating Data */

static gJSON *gJSON_ParseData(unsigned char *data, size_t length)
{
    parse_buffer buffer = { 0, 0, 0, 0 };
    gJSON *item = NULL;

    buffer.content = data;
    buffer.length = length;

    item = gJSON_New_Item();
    
    if(parse_value(item, &buffer) == false) {
        goto fail;
    }



    return item;

fail:
    if (item != NULL)
        gJSON_Delete(item);

    return NULL;
}

gJSON *gJSON_ParseWithLength(unsigned char *data, size_t length)
{
    return gJSON_ParseData(data, length);
}
