/* Adapt the vendor's error-returning primitive signatures, not its logic.
   rtl_phy_patch_request and generic_ocp_write share their Linux signatures. */
#define RTL_VER_17_QFN68 17
#define RTL_VER_17_QFN100 18
#ifndef PLA_GPHY_CTRL
#define PLA_GPHY_CTRL 0xd3ae
#endif
#ifndef GPHY_FLASH
#define GPHY_FLASH BIT(1)
#endif
#ifndef BYPASS_FLASH
#define BYPASS_FLASH BIT(5)
#endif
static int vendor_ocp_read_word(struct r8152*tp,unsigned bank,unsigned reg,u32*value)
{
    *value=ocp_read_word(tp,bank,reg);return 0;
}
static int vendor_ocp_word_set_bits(struct r8152*tp,unsigned bank,unsigned reg,u16 value)
{
    ocp_word_set_bits(tp,bank,reg,value);return 0;
}
static int vendor_sram_read(struct r8152*tp,unsigned reg,u16*value)
{
    *value=sram_read(tp,reg);return 0;
}
#define ocp_read_word vendor_ocp_read_word
#define ocp_word_set_bits vendor_ocp_word_set_bits
#define sram_read vendor_sram_read
#define rtl_ram_code_speed_up vendor_ram_code_speed_up
