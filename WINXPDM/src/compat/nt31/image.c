/* Read PE directories from the installed x86 NT 3.1 kernel and HAL.
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

typedef struct {
    PUCHAR bytes;
    ULONG rva, size, image_size;
} IMAGE_TABLE;

static BOOLEAN read_table(PCWSTR path, ULONG index, IMAGE_TABLE *table)
{
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK io;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    IMAGE_SECTION_HEADER section;
    IMAGE_DATA_DIRECTORY entry;
    HANDLE file;
    ULONG i, sections;
    BOOLEAN ok = FALSE;
    table->bytes = NULL;
    RtlInitUnicodeString(&name, path);
    InitializeObjectAttributes(&oa, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(ZwOpenFile(&file, FILE_READ_DATA | SYNCHRONIZE, &oa, &io,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT))) return FALSE;
    if (!read_at(file, 0, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || dos.e_lfanew > 0x100000 ||
        !read_at(file, dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.FileHeader.SizeOfOptionalHeader < sizeof(nt.OptionalHeader) ||
        nt.OptionalHeader.NumberOfRvaAndSizes <= index) goto done;
    entry = nt.OptionalHeader.DataDirectory[index];
    if (!entry.Size || entry.Size > 1024 * 1024 ||
        !range(entry.VirtualAddress, entry.Size, nt.OptionalHeader.SizeOfImage)) goto done;
    sections = dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        ULONG delta;
        if (!read_at(file, sections + i * sizeof(section), &section, sizeof(section))) break;
        if (entry.VirtualAddress < section.VirtualAddress) continue;
        delta = entry.VirtualAddress - section.VirtualAddress;
        if (!range(delta, entry.Size, section.SizeOfRawData)) continue;
        if (delta > MAXULONG - section.PointerToRawData) break;
        table->bytes = ExAllocatePool(PagedPool, entry.Size);
        if (table->bytes && read_at(file, section.PointerToRawData + delta, table->bytes, entry.Size)) {
            table->rva = entry.VirtualAddress;
            table->size = entry.Size;
            table->image_size = nt.OptionalHeader.SizeOfImage;
            ok = TRUE;
        } else if (table->bytes) {
            ExFreePool(table->bytes);
        }
        break;
    }
done:
    ZwClose(file);
    return ok;
}

static const WCHAR kernel_file[] = L"\\SystemRoot\\System32\\ntoskrnl.exe";
static const WCHAR hal_file[] = L"\\SystemRoot\\System32\\hal.dll";

BOOLEAN wxp_kernel_version(PULONG major, PULONG minor, PULONG build, PBOOLEAN checked)
{
    IMAGE_TABLE resources;
    if (!read_table(kernel_file, IMAGE_DIRECTORY_ENTRY_RESOURCE, &resources)) return FALSE;
    BOOLEAN ok = fixed_version(resources.bytes, resources.size, resources.rva, major, minor, build, checked);
    ExFreePool(resources.bytes);
    return ok;
}

static PVOID table_array(const IMAGE_TABLE *table, ULONG rva, ULONG count, ULONG stride)
{
    if (rva < table->rva || count > table->size / stride ||
        !range(rva - table->rva, count * stride, table->size)) return NULL;
    return table->bytes + (rva - table->rva);
}

static ULONG export_rva(const IMAGE_TABLE *table, PCWSTR name, ULONG length)
{
    const IMAGE_EXPORT_DIRECTORY *dir = (const IMAGE_EXPORT_DIRECTORY *)table->bytes;
    const ULONG *names, *functions;
    const USHORT *ordinals;
    if (table->size < sizeof(*dir)) return 0;
    names = table_array(table, dir->AddressOfNames, dir->NumberOfNames, sizeof(*names));
    functions = table_array(table, dir->AddressOfFunctions, dir->NumberOfFunctions, sizeof(*functions));
    ordinals = table_array(table, dir->AddressOfNameOrdinals, dir->NumberOfNames, sizeof(*ordinals));
    if (!names || !functions || !ordinals) return 0;
    for (ULONG i = 0; i < dir->NumberOfNames; ++i) {
        const UCHAR *s = table_array(table, names[i], length + 1, 1);
        ULONG j;
        if (!s || s[length] || ordinals[i] >= dir->NumberOfFunctions) continue;
        for (j = 0; j < length && name[j] == s[j]; ++j) {}
        if (j == length) {
            ULONG rva = functions[ordinals[i]];
            /* NT 3.1 has direct kernel and HAL exports. A forwarder is a string. */
            return rva < table->image_size && (rva < table->rva || rva - table->rva >= table->size) ? rva : 0;
        }
    }
    return 0;
}

typedef struct { PCWSTR name; PVOID *binding; } IMPORT_ANCHOR;
/* Use addresses bound by the native loader, independent of the preferred image base. */
extern PVOID bound_allocate_mdl __asm__("__imp__IoAllocateMdl@20");
extern PVOID bound_free_mdl __asm__("__imp__IoFreeMdl@4");
extern PVOID bound_get_bus __asm__("__imp__HalGetBusData@20");
extern PVOID bound_translate __asm__("__imp__HalTranslateBusAddress@24");
extern PVOID bound_get_bus_offset __asm__("__imp__HalGetBusDataByOffset@24");
extern PVOID bound_set_bus_offset __asm__("__imp__HalSetBusDataByOffset@24");
extern const struct { const CHAR *name; PVOID routine; } wxp_kernel_entries[];

static PVOID find_routine(PCWSTR file, const IMPORT_ANCHOR anchors[2], PUNICODE_STRING name)
{
    IMAGE_TABLE exports;
    ULONG_PTR base = 0;
    PVOID address = NULL;
    if (!read_table(file, IMAGE_DIRECTORY_ENTRY_EXPORT, &exports)) return NULL;
    ULONG rva = export_rva(&exports, name->Buffer, name->Length / sizeof(WCHAR));
    if (!rva) goto done;
    for (unsigned i = 0; i < 2; ++i) {
        UNICODE_STRING symbol;
        RtlInitUnicodeString(&symbol, anchors[i].name);
        ULONG offset = export_rva(&exports, symbol.Buffer, symbol.Length / sizeof(WCHAR));
        ULONG_PTR bound = (ULONG_PTR)*anchors[i].binding;
        if (!offset || bound < offset || ((bound - offset) & (PAGE_SIZE - 1))) goto done;
        if (i && bound - offset != base) goto done;
        base = bound - offset;
    }
    if (base <= MAXULONG_PTR - exports.image_size) address = (PVOID)(base + rva);
done:
    ExFreePool(exports.bytes);
    return address;
}

PVOID NTAPI WxpSystemRoutine(PUNICODE_STRING name)
{
    static const IMPORT_ANCHOR kernel[] = {{L"IoAllocateMdl",&bound_allocate_mdl}, {L"IoFreeMdl",&bound_free_mdl}};
    static const IMPORT_ANCHOR hal[] = {{L"HalGetBusData",&bound_get_bus}, {L"HalTranslateBusAddress",&bound_translate}};
    static const IMPORT_ANCHOR bridge[] = {{L"HalGetBusDataByOffset",&bound_get_bus_offset},
                                           {L"HalSetBusDataByOffset",&bound_set_bus_offset}};
    if (!name || !name->Buffer || !name->Length || name->Length & 1 ||
        name->Length > name->MaximumLength || KeGetCurrentIrql() != PASSIVE_LEVEL) return NULL;
    ULONG length = name->Length / sizeof(WCHAR);
    for (unsigned i = 0; wxp_kernel_entries[i].name; ++i) {
        const CHAR *s = wxp_kernel_entries[i].name;
        ULONG j = 0;
        while (j < length && s[j] && (UCHAR)s[j] == name->Buffer[j]) ++j;
        if (j == length && !s[j]) return wxp_kernel_entries[i].routine;
    }
    PVOID address = find_routine(kernel_file, kernel, name);
    if (!address) address = find_routine(hal_file, hal, name);
    return address ? address : find_routine(L"\\SystemRoot\\System32\\Drivers\\W3H.SYS", bridge, name);
}
