/* ===========================================================================
 * Neutron Kernel — String / Memory Utilities
 * =========================================================================== */

#ifndef NEUTRON_STRING_H
#define NEUTRON_STRING_H

#include <neutron/types.h>

void   *memset(void *dst, int c, usize n);
void   *memcpy(void *dst, const void *src, usize n);
void   *memmove(void *dst, const void *src, usize n);
int     memcmp(const void *a, const void *b, usize n);
usize   strlen(const char *s);
int     strcmp(const char *a, const char *b);
int     strncmp(const char *a, const char *b, usize n);
char   *strncpy(char *dst, const char *src, usize n);

#endif /* NEUTRON_STRING_H */
