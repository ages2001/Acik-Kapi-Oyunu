"""Build the 369-byte Windows 1.x/2.x standard-application settings record."""
import struct


def settings():
    image = bytearray(369)
    image[2:32] = b"WINRAM".ljust(30)
    struct.pack_into("<HH", image, 32, 384, 112)  # maximum and minimum DOS KiB
    image[36:99] = b"WINRAM.EXE\0".ljust(63)
    image[99] = 0x10
    image[165:229] = b"\0" * 64
    image[229:239] = bytes((127, 1, 0, 255, 25, 80, 0, 0, 23, 0))
    image[367:] = bytes((0xE0, 0x60))
    image[1] = sum(image[2:]) & 0xFF
    return bytes(image)
