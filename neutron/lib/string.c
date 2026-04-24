/* ===========================================================================
 * Neutron Kernel — String / Memory Utilities
 * =========================================================================== */

#include <neutron/string.h>

void *memset(void *dst, int c, usize n)
{
    u8 *d = (u8 *)dst;
    while (n--)
        *d++ = (u8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, usize n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, usize n)
{
    if (dst < src)
        return memcpy(dst, src, n);

    u8 *d = (u8 *)dst + n;
    const u8 *s = (const u8 *)src + n;
    while (n--)
        *--d = *--s;
    return dst;
}

int memcmp(const void *a, const void *b, usize n)
{
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    while (n--) {
        if (*pa != *pb)
            return (int)*pa - (int)*pb;
        pa++;
        pb++;
    }
    return 0;
}

usize strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (usize)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, usize n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char *strncpy(char *dst, const char *src, usize n)
{
    char *d = dst;
    while (n && *src) {
        *d++ = *src++;
        n--;
    }
    while (n--)
        *d++ = '\0';
    return dst;
}
