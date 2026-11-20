/* Only for the disposable 24 MiB FAT16 test disks. The data marker, four
   partition entries and every scratch sector are checked before the first
   write. The filesystem starts at sector 63. Sectors 16 to 31 are unused. */
static int blocks(HANDLE device, BYTE target, BYTE opcode, DWORD lba, BYTE sectors, BYTE *data) {
    Packet p;
    DWORD returned, size = sectors * 512u;
    char msg[180];
    ZeroMemory(&p, sizeof(p));
    p.request.length = sizeof(p.request);
    p.request.target = target;
    p.request.cdb_length = 10;
    p.request.sense_length = sizeof(p.sense);
    p.request.data_in = opcode == 0x28;
    p.request.bytes = size;
    p.request.timeout = 8;
    p.request.data_offset = offsetof(Packet, data);
    p.request.sense_offset = offsetof(Packet, sense);
    p.request.cdb[0] = opcode;
    p.request.cdb[2] = (BYTE)(lba >> 24);
    p.request.cdb[3] = (BYTE)(lba >> 16);
    p.request.cdb[4] = (BYTE)(lba >> 8);
    p.request.cdb[5] = (BYTE)lba;
    p.request.cdb[8] = sectors;
    if (opcode == 0x2a)
        CopyMemory(p.data, data, size);
    BOOL ok = DeviceIoControl(device, 0x4d004, &p, sizeof(p), &p, sizeof(p), &returned, 0);
    if (!ok || p.request.status || p.request.bytes != size ||
        (opcode == 0x28 && returned < offsetof(Packet, data) + size)) {
        wsprintfA(msg, "CDB %x target %u LBA %lu Win32 %lu SCSI %u transferred %lu", opcode, target,
                  lba, ok ? 0 : GetLastError(), p.request.status, p.request.bytes);
        line(msg);
        return 0;
    }
    if (opcode == 0x28)
        CopyMemory(data, p.data, size);
    return 1;
}
static void pattern(BYTE *data, DWORD lba, DWORD pass) {
    for (DWORD i = 0; i < 2048; ++i)
        data[i] = (BYTE)(0x61u ^ (i * 7u) ^ (i >> 8) ^ (lba * 13u) ^ (pass * 11u));
}
static int scratch(HANDLE port, BYTE id, int verify, const char *marker) {
    static BYTE b[2048], want[2048], zero[2048];
    char msg[140];
    DWORD pass, lba;
    if (!blocks(port, id, 0x28, 478, 1, b) || memcmp(b, marker, lstrlenA(marker)))
        return 0;
    if (!blocks(port, id, 0x28, 0, 1, b) || b[510] != 0x55 || b[511] != 0xaa || b[446 + 4] != 6 ||
        *(DWORD *)(b + 454) != 63 || *(DWORD *)(b + 458) != 49089 || memcmp(b + 462, zero, 48))
        return 0;
    if (!verify) {
        for (lba = 16; lba < 32; lba += 4) {
            if (!blocks(port, id, 0x28, lba, 4, b))
                return 0;
            if (memcmp(b, zero, 2048)) {
                line("Scratch sectors are already occupied; writes skipped");
                return 0;
            }
        }
        for (pass = 0; pass < 128; ++pass)
            for (lba = 16; lba < 32; lba += 4) {
                pattern(want, lba, pass);
                if (!blocks(port, id, 0x2a, lba, 4, want))
                    return 0;
            }
    }
    for (lba = 16; lba < 32; lba += 4) {
        pattern(want, lba, 127);
        if (!blocks(port, id, 0x28, lba, 4, b) || memcmp(b, want, 2048))
            return 0;
    }
    wsprintfA(msg, "Target %u %s LBAs 16-31, every byte matched", id,
              verify ? "post-reboot READ(10)" : "1048576 bytes WRITE(10) then READ(10)");
    line(msg);
    return 1;
}
