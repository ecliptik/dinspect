/* memory.h - base and extended memory detection */

#ifndef DOSFETCH_MEMORY_H
#define DOSFETCH_MEMORY_H

#include <stddef.h>

/* KB of contiguous conventional (base) memory, via INT 12h. */
unsigned get_base_memory_kb(void);

/* Formats "<kb> KB" of conventional memory into buf. */
void get_base_memory(char *buf, size_t buflen);

/* Formats extended memory size (KB) into buf, or "none" if undetectable. */
void get_extended_memory(char *buf, size_t buflen);

#endif
