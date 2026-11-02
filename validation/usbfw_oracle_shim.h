/* Test-only platform boundary for unchanged Linux firmware routines. */
/* Public USB vendor request constants from linux/usb/r8152.h. */
#define MCU_TYPE_USB 0x0000
#define MCU_TYPE_PLA 0x0100
#define BYTE_EN_DWORD 0xff
struct firmware {const u8 *data;unsigned size;};
struct r8152;
struct rtl_fw {
    const struct firmware *fw;char version[32];
    int (*pre_fw)(struct r8152*);int (*post_fw)(struct r8152*);
};
struct r8152 {unsigned version,flags;int ocp_base;struct rtl_fw rtl_fw;};
#define dev_err(...) ((void)0)
#define dev_dbg(...) ((void)0)
#define WARN_ON_ONCE(x) assert(!(x))
#define WARN_ON(x) assert(!(x))
#define IS_ERR_OR_NULL(x) (!(x))
#define test_bit(...) 0
#define usleep_range(a,b) ((void)(a),(void)(b))
#define strscpy(d,s,n) ((void)(d),(void)(s),(void)(n))
#define fallthrough __attribute__((fallthrough))
static unsigned bank(unsigned type){assert(!type||type==0x100);return type?1:0;}
static u32 ocp_read_value(struct r8152*tp,unsigned type,unsigned addr,unsigned n)
{
    u8 b[4]={0};(void)tp;assert(io_read(0,bank(type),addr,b,n));
    return b[0]|(u32)b[1]<<8|(u32)b[2]<<16|(u32)b[3]<<24;
}
static u8 ocp_read_byte(struct r8152*tp,unsigned type,unsigned addr){return ocp_read_value(tp,type,addr,1);}
static u16 ocp_read_word(struct r8152*tp,unsigned type,unsigned addr){return ocp_read_value(tp,type,addr,2);}
static void ocp_write_value(struct r8152*tp,unsigned type,unsigned addr,u32 v,unsigned n)
{
    u8 b[4]={v,v>>8,v>>16,v>>24};(void)tp;assert(io_write(0,bank(type),addr,b,n));
}
static void ocp_write_byte(struct r8152*tp,unsigned type,unsigned addr,u8 v){ocp_write_value(tp,type,addr,v,1);}
static void ocp_write_word(struct r8152*tp,unsigned type,unsigned addr,u16 v){ocp_write_value(tp,type,addr,v,2);}
static void ocp_write_dword(struct r8152*tp,unsigned type,unsigned addr,u32 v){ocp_write_value(tp,type,addr,v,4);}
static void ocp_word_set_bits(struct r8152*tp,unsigned type,unsigned addr,u16 v){ocp_write_word(tp,type,addr,ocp_read_word(tp,type,addr)|v);}
static void ocp_word_clr_bits(struct r8152*tp,unsigned type,unsigned addr,u16 v){ocp_write_word(tp,type,addr,ocp_read_word(tp,type,addr)&~v);}
static void ocp_byte_set_bits(struct r8152*tp,unsigned type,unsigned addr,u8 v){ocp_write_byte(tp,type,addr,ocp_read_byte(tp,type,addr)|v);}
static int generic_ocp_write(struct r8152*tp,unsigned addr,unsigned enable,unsigned n,const void*p,unsigned type)
{
    (void)tp;(void)enable;assert(io_write(0,bank(type),addr,p,n));return 0;
}
static u16 phy_read(unsigned bank,unsigned addr)
{
    u8 b[2];assert(io_read(0,bank,addr,b,2));return b[0]|(unsigned)b[1]<<8;
}
static void phy_write(unsigned bank,unsigned addr,u16 v)
{
    u8 b[2]={v,v>>8};assert(io_write(0,bank,addr,b,2));
}
static u16 ocp_reg_read(struct r8152*tp,unsigned addr){(void)tp;return phy_read(2,addr);}
static void ocp_reg_write(struct r8152*tp,unsigned addr,u16 v){(void)tp;phy_write(2,addr,v);}
static void ocp_reg_set_bits(struct r8152*tp,unsigned addr,u16 v){ocp_reg_write(tp,addr,ocp_reg_read(tp,addr)|v);}
static void ocp_reg_clr_bits(struct r8152*tp,unsigned addr,u16 v){ocp_reg_write(tp,addr,ocp_reg_read(tp,addr)&~v);}
static u16 sram_read(struct r8152*tp,unsigned addr){(void)tp;return phy_read(3,addr);}
static void sram_write(struct r8152*tp,unsigned addr,u16 v){(void)tp;phy_write(3,addr,v);}
