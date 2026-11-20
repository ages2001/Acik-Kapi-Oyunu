#include <stddef.h>

/* Used by the DDK inline stack-copy routine in both freestanding images. */
void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *to = destination;
    const unsigned char *from = source;
    while (count--)
        *to++ = *from++;
    return destination;
}

#ifdef WK_LOCAL_MEMSET
/* NT 3.1 also needs a local fill routine for the KS class driver and client. */
void *memset(void *destination, int value, size_t count)
{
    unsigned char *to = destination;
    while (count--)
        *to++ = (unsigned char)value;
    return destination;
}
#endif
