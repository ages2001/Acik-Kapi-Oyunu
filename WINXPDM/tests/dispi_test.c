/* Test binding to the I/O ports of a running QEMU machine. */
typedef unsigned short (*read_reg)(unsigned short);
typedef void (*write_reg)(unsigned short, unsigned short);
static read_reg dispi_rd;
static write_reg dispi_wr;
#include "../src/display/dispi.h"

unsigned long test_mode(read_reg read, write_reg write, unsigned short width,
                        unsigned short height, unsigned short depth, unsigned short flags)
{
    dispi_rd = read;
    dispi_wr = write;
    return wxp_set_dispi(width, height, depth, flags);
}
