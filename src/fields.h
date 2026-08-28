/* fields.h - a detected system fact, ready to render to screen or file */

#ifndef DOSFETCH_FIELDS_H
#define DOSFETCH_FIELDS_H

#define MAX_FIELDS      28
#define FIELD_VALUE_LEN 80

typedef struct {
    const char *label;
    char value[FIELD_VALUE_LEN];
} field_t;

#endif
