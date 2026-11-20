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
