/* Read the installed NT 3.1 kernel's version before the SOFTWARE hive is mounted.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#include <ddk/ntimage.h>

static BOOLEAN read_at(HANDLE file, ULONG offset, PVOID buffer, ULONG bytes)
{
    LARGE_INTEGER pos;
    IO_STATUS_BLOCK io;
    pos.QuadPart = offset;
    return NT_SUCCESS(ZwReadFile(file, NULL, NULL, NULL, &io, buffer, bytes, &pos, NULL)) &&
           io.Information == bytes;
}

static BOOLEAN range(ULONG offset, ULONG length, ULONG size)
{
    return offset <= size && length <= size - offset;
}

static BOOLEAN directory(PUCHAR root, ULONG size, ULONG *offset, ULONG id, BOOLEAN branch)
{
    PIMAGE_RESOURCE_DIRECTORY dir;
    /* Each PE resource entry is a name/ID followed by a tagged offset. */
    struct resource_entry { ULONG id, target; } *entries;
    ULONG count, i;
    if (!range(*offset, sizeof(*dir), size)) return FALSE;
    dir = (PIMAGE_RESOURCE_DIRECTORY)(root + *offset);
    count = (ULONG)dir->NumberOfNamedEntries + dir->NumberOfIdEntries;
    if (!range(*offset + sizeof(*dir), count * sizeof(*entries), size)) return FALSE;
    entries = (struct resource_entry *)(dir + 1);
    for (i = dir->NumberOfNamedEntries; i < count; ++i) {
        if ((!id || entries[i].id == id) && (entries[i].target >> 31) == branch) {
            *offset = entries[i].target & 0x7fffffff;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN fixed_version(PUCHAR root, ULONG size, ULONG base, PULONG major, PULONG minor,
                             PULONG build, PBOOLEAN checked)
{
    static const WCHAR key[] = L"VS_VERSION_INFO";
    PIMAGE_RESOURCE_DATA_ENTRY data;
    ULONG at = 0, i, *fixed;
    USHORT *header;
    if (!directory(root, size, &at, 16, TRUE) || !directory(root, size, &at, 1, TRUE) ||
        !directory(root, size, &at, 0, FALSE) || !range(at, sizeof(*data), size)) return FALSE;
    data = (PIMAGE_RESOURCE_DATA_ENTRY)(root + at);
    if (data->OffsetToData < base || !range(data->OffsetToData - base, data->Size, size)) return FALSE;
    at = data->OffsetToData - base;
    if (data->Size < 40 + 13 * sizeof(ULONG)) return FALSE;
    header = (USHORT *)(root + at);
    if (header[0] > data->Size || header[0] < 92 || header[1] < 52) return FALSE;
    for (i = 0; i < sizeof(key) / sizeof(*key); ++i) if (header[3 + i] != key[i]) return FALSE;
    fixed = (ULONG *)(root + at + 40);
    if (fixed[0] != 0xfeef04bd || fixed[1] != 0x10000) return FALSE;
    *major = fixed[2] >> 16;
    *minor = fixed[2] & 0xffff;
    *build = fixed[3] >> 16;
    *checked = (fixed[6] & fixed[7] & 1) != 0;
    return TRUE;
}

BOOLEAN wxp_kernel_version(PULONG major, PULONG minor, PULONG build, PBOOLEAN checked)
{
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK io;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    IMAGE_SECTION_HEADER section;
    IMAGE_DATA_DIRECTORY resources;
    HANDLE file;
    ULONG i, sections;
    PUCHAR root = NULL;
    BOOLEAN ok = FALSE;
    RtlInitUnicodeString(&name, L"\\SystemRoot\\System32\\ntoskrnl.exe");
    InitializeObjectAttributes(&oa, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(ZwOpenFile(&file, FILE_READ_DATA | SYNCHRONIZE, &oa, &io,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT))) return FALSE;
    if (!read_at(file, 0, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 0x100000 ||
        !read_at(file, dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.FileHeader.SizeOfOptionalHeader < sizeof(nt.OptionalHeader) ||
        nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) goto done;
    resources = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    sections = dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        ULONG delta;
        if (!read_at(file, sections + i * sizeof(section), &section, sizeof(section))) break;
        if (resources.VirtualAddress < section.VirtualAddress) continue;
        delta = resources.VirtualAddress - section.VirtualAddress;
        if (!range(delta, resources.Size, section.SizeOfRawData)) continue;
        if (!resources.Size || resources.Size > 1024 * 1024 || delta > MAXULONG - section.PointerToRawData) break;
        root = ExAllocatePool(PagedPool, resources.Size);
        if (root && read_at(file, section.PointerToRawData + delta, root, resources.Size))
            ok = fixed_version(root, resources.Size, resources.VirtualAddress, major, minor, build, checked);
        break;
    }
done:
    if (root) ExFreePool(root);
    ZwClose(file);
    return ok;
}
