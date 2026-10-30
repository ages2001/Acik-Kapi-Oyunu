/* Compiler-emitted memory helper; no CRT dependency on the target OS. */
void *memset(void *dst,int value,unsigned count){volatile unsigned char *p=dst;while(count--)*p++=(unsigned char)value;return dst;}
