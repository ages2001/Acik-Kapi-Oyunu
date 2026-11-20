#ifndef N3_PATCH_H
#define N3_PATCH_H
#include <stddef.h>
#include <stdint.h>
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t u32;
#define MAX_PORT (4u * 1024u * 1024u)
#define EXTRA 131072u
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
int strcmp(const char *, const char *);
size_t strlen(const char *);
/* The output buffer must hold input size + EXTRA bytes. No file is touched. */
void sha256(const byte *, u32, char [65]);
const char *bridge(const byte *input, u32 size, byte *out, u32 *output_size);
const char *port_imports(const byte *, u32, const byte *, u32);
#endif
