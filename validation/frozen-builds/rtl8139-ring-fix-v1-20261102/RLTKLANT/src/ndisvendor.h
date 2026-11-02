/* OID_GEN_VENDOR_ID is three IEEE address bytes followed by one vendor byte.
 * Use the permanent address prefix, including OEM OUIs; never substitute a
 * PCI vendor number, a reversed numeric OUI, or the configured station MAC.
 * All supported targets are little-endian x86/x64. The final byte is zero.
 */
#ifndef RLTK_NDIS_VENDOR_H
#define RLTK_NDIS_VENDOR_H
static unsigned long RtlNdisVendorId(const unsigned char *permanent)
{
    return (unsigned long)permanent[0] |
           ((unsigned long)permanent[1] << 8) |
           ((unsigned long)permanent[2] << 16);
}
#endif
