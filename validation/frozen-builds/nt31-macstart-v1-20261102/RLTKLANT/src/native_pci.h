/* NT5 PCI configuration I/O, isolated from the private NDIS header ABI.
 * PASSIVE_LEVEL only. The owner must retain the PDO and join all users
 * before Halt/removal releases that device reference. */
#ifndef RTL_NATIVE_PCI_H
#define RTL_NATIVE_PCI_H
void *RtlNativePciRetain(void *pdo);
void RtlNativePciRelease(void *pdo);
int RtlNativePciConfig(void *pdo,int write,unsigned short offset,
                       unsigned width,unsigned int *value);
int RtlNativePciFunction(void *pdo,unsigned char *function);
#endif
