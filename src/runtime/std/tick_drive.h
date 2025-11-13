#ifndef TICK_DRIVE_H_
#define TICK_DRIVE_H_
// tick_drive.h - Drive and partition table structures (MBR, GPT)
// Freestanding header for reading/writing partition tables

#include <stddef.h>
#include <stdint.h>

// Type aliases matching Tick primitive types
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ptrdiff_t isz;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usz;

// ============================================================================
// MBR (Master Boot Record) Structures
// ============================================================================

// CHS (Cylinder-Head-Sector) address format
typedef struct __attribute__((packed)) {
  u8 head;
  u8 sector : 6;     // Lower 6 bits: sector (1-63)
  u8 cylinder_hi : 2;  // Upper 2 bits: high bits of cylinder
  u8 cylinder_lo;    // Lower 8 bits of cylinder
} tick_mbr_chs_t;

// MBR Partition Table Entry (16 bytes)
typedef struct __attribute__((packed)) {
  u8 boot_indicator;       // 0x80 = bootable, 0x00 = non-bootable
  tick_mbr_chs_t start_chs;  // Starting CHS address (legacy)
  u8 partition_type;       // Partition type code
  tick_mbr_chs_t end_chs;    // Ending CHS address (legacy)
  u32 start_lba;           // Starting LBA (Logical Block Address)
  u32 size_sectors;        // Size in sectors
} tick_mbr_partition_t;

// MBR (Master Boot Record) - 512 bytes
typedef struct __attribute__((packed)) {
  u8 bootstrap_code[440];        // Bootstrap code area
  u32 disk_signature;            // Optional disk signature
  u16 reserved;                  // Usually 0x0000
  tick_mbr_partition_t partitions[4];  // 4 partition entries
  u16 boot_signature;            // 0xAA55 boot signature
} tick_mbr_t;

// Common MBR partition type codes
#define TICK_MBR_TYPE_EMPTY 0x00
#define TICK_MBR_TYPE_FAT12 0x01
#define TICK_MBR_TYPE_FAT16_SMALL 0x04
#define TICK_MBR_TYPE_EXTENDED 0x05
#define TICK_MBR_TYPE_FAT16 0x06
#define TICK_MBR_TYPE_NTFS 0x07
#define TICK_MBR_TYPE_FAT32_CHS 0x0B
#define TICK_MBR_TYPE_FAT32_LBA 0x0C
#define TICK_MBR_TYPE_FAT16_LBA 0x0E
#define TICK_MBR_TYPE_EXTENDED_LBA 0x0F
#define TICK_MBR_TYPE_LINUX_SWAP 0x82
#define TICK_MBR_TYPE_LINUX 0x83
#define TICK_MBR_TYPE_LINUX_EXTENDED 0x85
#define TICK_MBR_TYPE_LINUX_LVM 0x8E
#define TICK_MBR_TYPE_GPT_PROTECTIVE 0xEE
#define TICK_MBR_TYPE_EFI_SYSTEM 0xEF

// MBR boot signature constant
#define TICK_MBR_BOOT_SIGNATURE 0xAA55

// ============================================================================
// GPT (GUID Partition Table) Structures
// ============================================================================

// GUID structure (16 bytes)
typedef struct __attribute__((packed)) {
  u32 data1;
  u16 data2;
  u16 data3;
  u8 data4[8];
} tick_guid_t;

// GPT Header - 512 bytes (with reserved padding)
typedef struct __attribute__((packed)) {
  u64 signature;          // "EFI PART" (0x5452415020494645)
  u32 revision;           // GPT revision (typically 0x00010000)
  u32 header_size;        // Header size in bytes (typically 92)
  u32 header_crc32;       // CRC32 of header (with this field zeroed)
  u32 reserved;           // Must be zero
  u64 current_lba;        // LBA of this header
  u64 backup_lba;         // LBA of backup header
  u64 first_usable_lba;   // First usable LBA for partitions
  u64 last_usable_lba;    // Last usable LBA for partitions
  tick_guid_t disk_guid;  // Disk GUID
  u64 partition_entries_lba;     // Starting LBA of partition entries
  u32 num_partition_entries;     // Number of partition entries
  u32 partition_entry_size;      // Size of each partition entry (typically 128)
  u32 partition_array_crc32;     // CRC32 of partition array
  u8 reserved_padding[420];      // Reserved (zeros) - pads to 512 bytes
} tick_gpt_header_t;

// GPT Partition Entry - 128 bytes
typedef struct __attribute__((packed)) {
  tick_guid_t partition_type_guid;  // Partition type GUID
  tick_guid_t unique_partition_guid;  // Unique partition GUID
  u64 starting_lba;                 // Starting LBA
  u64 ending_lba;                   // Ending LBA (inclusive)
  u64 attributes;                   // Attribute flags
  u16 partition_name[36];           // Partition name (UTF-16LE, 72 bytes)
} tick_gpt_partition_t;

// GPT signature constant "EFI PART"
#define TICK_GPT_SIGNATURE 0x5452415020494645ULL

// GPT revision
#define TICK_GPT_REVISION 0x00010000

// GPT partition attribute flags
#define TICK_GPT_ATTR_REQUIRED_PARTITION (1ULL << 0)
#define TICK_GPT_ATTR_NO_BLOCK_IO_PROTOCOL (1ULL << 1)
#define TICK_GPT_ATTR_LEGACY_BIOS_BOOTABLE (1ULL << 2)

// Common GPT partition type GUIDs (stored in little-endian format)

// Unused entry
#define TICK_GPT_TYPE_UNUSED                                                   \
  ((tick_guid_t){0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})

// EFI System Partition
#define TICK_GPT_TYPE_EFI_SYSTEM                                               \
  ((tick_guid_t){0xC12A7328, 0xF81F, 0x11D2, {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}})

// Linux filesystem
#define TICK_GPT_TYPE_LINUX_FILESYSTEM                                         \
  ((tick_guid_t){0x0FC63DAF, 0x8483, 0x4772, {0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4}})

// Linux swap
#define TICK_GPT_TYPE_LINUX_SWAP                                               \
  ((tick_guid_t){0x0657FD6D, 0xA4AB, 0x43C4, {0x84, 0xE5, 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F}})

// Linux LVM
#define TICK_GPT_TYPE_LINUX_LVM                                                \
  ((tick_guid_t){0xE6D6D379, 0xF507, 0x44C2, {0xA2, 0x3C, 0x23, 0x8F, 0x2A, 0x3D, 0xF9, 0x28}})

// Microsoft Basic Data
#define TICK_GPT_TYPE_MICROSOFT_BASIC_DATA                                     \
  ((tick_guid_t){0xEBD0A0A2, 0xB9E5, 0x4433, {0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}})

// Microsoft Reserved
#define TICK_GPT_TYPE_MICROSOFT_RESERVED                                       \
  ((tick_guid_t){0xE3C9E316, 0x0B5C, 0x4DB8, {0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE}})

// Apple HFS/HFS+
#define TICK_GPT_TYPE_APPLE_HFS                                                \
  ((tick_guid_t){0x48465300, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}})

// Apple APFS
#define TICK_GPT_TYPE_APPLE_APFS                                               \
  ((tick_guid_t){0x7C3457EF, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}})

#endif  // TICK_DRIVE_H_
