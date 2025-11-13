// tick_drive.c - Drive and partition table functions
// Implementation of MBR and GPT validation, initialization, and utilities

#include "tick_drive.h"

// ============================================================================
// CRC32 Implementation (IEEE 802.3, polynomial 0x04C11DB7)
// ============================================================================

// CRC32 lookup table for polynomial 0xEDB88320 (reversed 0x04C11DB7)

static const u32 tick_crc32_table[256] =
  {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
  };

// Calculate CRC32 checksum (IEEE 802.3 polynomial)
u32 tick_crc32(const void *data, usz len) {
  const u8 *buf = (const u8 *)data;
  u32 crc = 0xFFFFFFFF;

  for (usz i = 0; i < len; i++) {
    crc = tick_crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  }

  return ~crc;
}

// ============================================================================
// GUID Utilities
// ============================================================================

// Compare two GUIDs for equality
bool tick_guid_equal(tick_guid_t a, tick_guid_t b) {
  return a.data1 == b.data1 && a.data2 == b.data2 && a.data3 == b.data3 &&
         a.data4[0] == b.data4[0] && a.data4[1] == b.data4[1] &&
         a.data4[2] == b.data4[2] && a.data4[3] == b.data4[3] &&
         a.data4[4] == b.data4[4] && a.data4[5] == b.data4[5] &&
         a.data4[6] == b.data4[6] && a.data4[7] == b.data4[7];
}

// Check if GUID is all zeros
bool tick_guid_is_zero(tick_guid_t g) {
  return g.data1 == 0 && g.data2 == 0 && g.data3 == 0 && g.data4[0] == 0 &&
         g.data4[1] == 0 && g.data4[2] == 0 && g.data4[3] == 0 &&
         g.data4[4] == 0 && g.data4[5] == 0 && g.data4[6] == 0 &&
         g.data4[7] == 0;
}

// ============================================================================
// MBR Functions
// ============================================================================

// Validate MBR structure
bool tick_mbr_validate(const tick_mbr_t *mbr) {
  // Check boot signature
  if (mbr->boot_signature != TICK_MBR_BOOT_SIGNATURE) {
    return false;
  }

  // Check for protective MBR (GPT disk)
  // If first partition is type 0xEE, this is a GPT disk with protective MBR
  // This is still a valid MBR structure

  return true;
}

// Initialize MBR with default values
void tick_mbr_init(tick_mbr_t *mbr) {
  *mbr = (tick_mbr_t){0};
  mbr->boot_signature = TICK_MBR_BOOT_SIGNATURE;

  // Initialize all partitions as empty
  for (int i = 0; i < 4; i++) {
    mbr->partitions[i].partition_type = TICK_MBR_TYPE_EMPTY;
  }
}

// Convert CHS address to LBA
// Note: CHS addressing is legacy and limited to ~8GB disks
u32 tick_mbr_chs_to_lba(tick_mbr_chs_t chs, u8 heads, u8 sectors_per_track) {
  u16 cylinder = ((u16)chs.cylinder_hi << 8) | chs.cylinder_lo;
  u8 head = chs.head;
  u8 sector = chs.sector; // Sectors are 1-based (1-63)

  // LBA = (C × HPC + H) × SPT + (S − 1)
  // where C = cylinder, H = head, S = sector
  // HPC = heads per cylinder, SPT = sectors per track
  return (cylinder * heads + head) * sectors_per_track + (sector - 1);
}

// Convert LBA to CHS address
// Note: This requires knowledge of disk geometry
tick_mbr_chs_t tick_mbr_lba_to_chs(u32 lba, u8 heads, u8 sectors_per_track) {
  tick_mbr_chs_t chs;

  // Calculate CHS from LBA
  // C = LBA ÷ (HPC × SPT)
  // H = (LBA ÷ SPT) mod HPC
  // S = (LBA mod SPT) + 1
  u16 cylinder = lba / (heads * sectors_per_track);
  u8 head = (lba / sectors_per_track) % heads;
  u8 sector = (lba % sectors_per_track) + 1;

  // Clamp cylinder to 10 bits (max 1023)
  if (cylinder > 1023) {
    cylinder = 1023;
  }

  // Clamp sector to 6 bits (max 63)
  if (sector > 63) {
    sector = 63;
  }

  chs.head = head;
  chs.sector = sector & 0x3F;           // Lower 6 bits
  chs.cylinder_hi = (cylinder >> 8) & 0x03; // Upper 2 bits of cylinder
  chs.cylinder_lo = cylinder & 0xFF;    // Lower 8 bits of cylinder

  return chs;
}

// ============================================================================
// GPT Functions
// ============================================================================

// Validate GPT header
bool tick_gpt_header_validate(const tick_gpt_header_t *hdr) {
  // Check signature
  if (hdr->signature != TICK_GPT_SIGNATURE) {
    return false;
  }

  // Check revision
  if (hdr->revision != TICK_GPT_REVISION) {
    return false;
  }

  // Check header size (should be 92 bytes for GPT 1.0)
  if (hdr->header_size < 92) {
    return false;
  }

  // Verify header CRC32
  u32 saved_crc = hdr->header_crc32;
  tick_gpt_header_t temp = *hdr;
  temp.header_crc32 = 0;

  u32 calculated_crc = tick_crc32(&temp, hdr->header_size);
  if (calculated_crc != saved_crc) {
    return false;
  }

  return true;
}

// Initialize GPT header
void tick_gpt_header_init(tick_gpt_header_t *hdr, u64 disk_size_sectors,
                           tick_guid_t disk_guid) {
  *hdr = (tick_gpt_header_t){0};

  hdr->signature = TICK_GPT_SIGNATURE;
  hdr->revision = TICK_GPT_REVISION;
  hdr->header_size = 92; // Standard GPT header size
  hdr->header_crc32 = 0; // Will be calculated later
  hdr->reserved = 0;

  // Primary GPT header is at LBA 1
  hdr->current_lba = 1;

  // Backup GPT header is at last LBA
  hdr->backup_lba = disk_size_sectors - 1;

  // First usable LBA (after primary GPT + partition entries)
  // Typically: LBA 34 (1 + 1 + 32 partition entry sectors)
  hdr->first_usable_lba = 34;

  // Last usable LBA (before backup partition entries)
  // Typically: disk_size - 34
  hdr->last_usable_lba = disk_size_sectors - 34;

  // Set disk GUID
  hdr->disk_guid = disk_guid;

  // Partition entries start at LBA 2 (right after header)
  hdr->partition_entries_lba = 2;

  // Standard: 128 partition entries
  hdr->num_partition_entries = 128;

  // Standard: 128 bytes per entry
  hdr->partition_entry_size = 128;

  // Partition array CRC32 will be calculated separately
  hdr->partition_array_crc32 = 0;
}

// Calculate CRC32 for GPT header
u32 tick_gpt_header_calc_crc32(const tick_gpt_header_t *hdr) {
  tick_gpt_header_t temp = *hdr;
  temp.header_crc32 = 0;

  return tick_crc32(&temp, hdr->header_size);
}

// Calculate CRC32 for GPT partition array
u32 tick_gpt_partition_array_calc_crc32(const tick_gpt_partition_t *parts,
                                         u32 num_entries, u32 entry_size) {
  // Calculate CRC32 over the entire partition array
  // Note: entry_size allows for future extensions beyond 128 bytes
  usz array_size = (usz)num_entries * entry_size;
  return tick_crc32(parts, array_size);
}

// Check if GPT partition entry is in use
bool tick_gpt_partition_is_used(const tick_gpt_partition_t *part) {
  // A partition is unused if its type GUID is all zeros
  return !tick_guid_is_zero(part->partition_type_guid);
}

// ============================================================================
// UTF-16LE String Utilities
// ============================================================================

// Convert UTF-8 to UTF-16LE for GPT partition names
// Returns number of UTF-16 units written (not including null terminator)
// max_utf16_units should be 36 for GPT partition names
usz tick_utf8_to_utf16le(const char *utf8, u16 *utf16, usz max_utf16_units) {
  if (!utf8 || !utf16 || max_utf16_units == 0) {
    return 0;
  }

  const u8 *src = (const u8 *)utf8;
  usz out_pos = 0;

  while (*src && out_pos < max_utf16_units) {
    u32 codepoint;
    usz bytes_read;

    // Decode UTF-8 codepoint
    if ((*src & 0x80) == 0) {
      // 1-byte sequence (0xxxxxxx)
      codepoint = *src;
      bytes_read = 1;
    } else if ((*src & 0xE0) == 0xC0) {
      // 2-byte sequence (110xxxxx 10xxxxxx)
      if ((src[1] & 0xC0) != 0x80) break; // Invalid
      codepoint = ((src[0] & 0x1F) << 6) | (src[1] & 0x3F);
      bytes_read = 2;
    } else if ((*src & 0xF0) == 0xE0) {
      // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
      if ((src[1] & 0xC0) != 0x80 || (src[2] & 0xC0) != 0x80) break;
      codepoint = ((src[0] & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
      bytes_read = 3;
    } else if ((*src & 0xF8) == 0xF0) {
      // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
      if ((src[1] & 0xC0) != 0x80 || (src[2] & 0xC0) != 0x80 || (src[3] & 0xC0) != 0x80) break;
      codepoint = ((src[0] & 0x07) << 18) | ((src[1] & 0x3F) << 12) |
                  ((src[2] & 0x3F) << 6) | (src[3] & 0x3F);
      bytes_read = 4;
    } else {
      // Invalid UTF-8 sequence
      break;
    }

    // Encode as UTF-16LE
    if (codepoint < 0x10000) {
      // BMP (Basic Multilingual Plane) - direct encoding
      if (out_pos >= max_utf16_units) break;
      utf16[out_pos++] = (u16)codepoint;
    } else if (codepoint <= 0x10FFFF) {
      // Supplementary planes - use surrogate pair
      if (out_pos + 1 >= max_utf16_units) break;
      codepoint -= 0x10000;
      utf16[out_pos++] = (u16)(0xD800 + (codepoint >> 10));    // High surrogate
      utf16[out_pos++] = (u16)(0xDC00 + (codepoint & 0x3FF));  // Low surrogate
    } else {
      // Invalid codepoint
      break;
    }

    src += bytes_read;
  }

  // Pad remaining with zeros
  for (usz i = out_pos; i < max_utf16_units; i++) {
    utf16[i] = 0;
  }

  return out_pos;
}

// Convert UTF-16LE to UTF-8 for reading GPT partition names
// Returns number of bytes written to utf8 buffer (not including null terminator)
usz tick_utf16le_to_utf8(const u16 *utf16, usz utf16_len, char *utf8,
                          usz max_bytes) {
  if (!utf16 || !utf8 || max_bytes == 0) {
    return 0;
  }

  u8 *dst = (u8 *)utf8;
  usz out_pos = 0;
  usz in_pos = 0;

  while (in_pos < utf16_len && utf16[in_pos] != 0 && out_pos < max_bytes - 1) {
    u32 codepoint;

    u16 unit = utf16[in_pos++];

    // Check for surrogate pair
    if (unit >= 0xD800 && unit <= 0xDBFF) {
      // High surrogate - need low surrogate
      if (in_pos >= utf16_len) break;
      u16 low = utf16[in_pos];
      if (low >= 0xDC00 && low <= 0xDFFF) {
        in_pos++;
        codepoint = 0x10000 + (((u32)(unit - 0xD800) << 10) | (low - 0xDC00));
      } else {
        // Invalid surrogate pair
        break;
      }
    } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
      // Low surrogate without high surrogate - invalid
      break;
    } else {
      // BMP codepoint
      codepoint = unit;
    }

    // Encode as UTF-8
    if (codepoint < 0x80) {
      // 1-byte sequence
      if (out_pos >= max_bytes - 1) break;
      dst[out_pos++] = (u8)codepoint;
    } else if (codepoint < 0x800) {
      // 2-byte sequence
      if (out_pos + 1 >= max_bytes - 1) break;
      dst[out_pos++] = (u8)(0xC0 | (codepoint >> 6));
      dst[out_pos++] = (u8)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
      // 3-byte sequence
      if (out_pos + 2 >= max_bytes - 1) break;
      dst[out_pos++] = (u8)(0xE0 | (codepoint >> 12));
      dst[out_pos++] = (u8)(0x80 | ((codepoint >> 6) & 0x3F));
      dst[out_pos++] = (u8)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
      // 4-byte sequence
      if (out_pos + 3 >= max_bytes - 1) break;
      dst[out_pos++] = (u8)(0xF0 | (codepoint >> 18));
      dst[out_pos++] = (u8)(0x80 | ((codepoint >> 12) & 0x3F));
      dst[out_pos++] = (u8)(0x80 | ((codepoint >> 6) & 0x3F));
      dst[out_pos++] = (u8)(0x80 | (codepoint & 0x3F));
    }
  }

  // Null terminate
  dst[out_pos] = 0;

  return out_pos;
}
