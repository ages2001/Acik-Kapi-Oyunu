#define WIN32_LEAN_AND_MEAN
#include "wx_firmware.h"
#include "../test_output.h"

void WINAPI identity_entry(void)
{
    struct wx_system_identity data;
    struct wx_firmware_identity firmware;
    initialize_test();
    if (!wx_read_system_identity(&data)) ExitProcess(1);
    wx_read_firmware_identity(&firmware);
    report("Operating system: "); report(data.os);
    report("\r\nProcessor: "); report(data.processor);
    report("\r\nSystem manufacturer: "); report(firmware.manufacturer);
    report("\r\nSystem model: "); report(firmware.model);
    report("\r\nBIOS: "); report(firmware.bios);
    report("\r\nRegistry product name: "); report(data.product_name);
    report("\r\nCPUID vendor: "); report(data.vendor);
    report("\r\nCPUID signature: "); report_hex(data.signature);
    report("\r\nCPUID or registry brand: "); report(data.brand);
    report("\r\n");
    ExitProcess(!data.os[0] || !data.processor[0] || !data.logical_processors);
}
