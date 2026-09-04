/* fileout.c - plain-text report writer */

#include <stdio.h>
#include "fileout.h"

int write_fields_file(const field_t *fields, int count, const char *path)
{
    FILE *fp;
    int i;

    fp = fopen(path, "w");
    if (fp == NULL)
        return -1;

    for (i = 0; i < count; i++)
        fprintf(fp, "%s: %s\n", fields[i].label, fields[i].value);

    fclose(fp);
    return 0;
}

int truncate_fields_file(const char *path)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL)
        return -1;

    fclose(fp);
    return 0;
}
