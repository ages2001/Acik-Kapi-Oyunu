#include "patch.h"
void *memcpy(void *to, const void *from, size_t n) {
    byte *out = to;
    const byte *in = from;
    while (n--)
        *out++ = *in++;
    return to;
}
void *memset(void *to, int v, size_t n) {
    byte *p = to;
    while (n--)
        *p++ = (byte)v;
    return to;
}
int memcmp(const void *a, const void *b, size_t n) {
    const byte *p = a, *q = b;
    while (n--) {
        if (*p != *q)
            return *p - *q;
        ++p;
        ++q;
    }
    return 0;
}
size_t strlen(const char *p) {
    const char *q = p;
    while (*q)
        ++q;
    return (size_t)(q - p);
}
int strcmp(const char *p, const char *q) {
    while (*p && *p == *q) {
        ++p;
        ++q;
    }
    return (byte)*p - (byte)*q;
}
