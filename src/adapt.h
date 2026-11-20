#ifndef NT3_ADAPT_H
#define NT3_ADAPT_H
#include "patch.h"
#define INPUT_MAX (4u * 1024u * 1024u)
#define LINK_SPACE 131072u
/* A standalone PE adapter embeds a complete compact x86 SCSI miniport.
   The caller supplies size + LINK_SPACE bytes. major/minor are checked by setup. */
const char *adapter(const byte *source, u32 size, unsigned minor, byte *out, u32 *used);
#endif
