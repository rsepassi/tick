#!/usr/bin/env python3
import sys
import struct

NTP_TO_UNIX = 2208988800
TAI64_BASE = 0x4000000000000000

def parse_leap_seconds(filename):
    leapsecs = []
    for line in open(filename):
        line = line.strip()
        if line and not line.startswith('#'):
            parts = line.split()
            leapsecs.append((int(parts[0]), int(parts[1])))
    return leapsecs

input_file = sys.argv[1] if len(sys.argv) > 1 else "leap-seconds.list"
output_file = sys.argv[2] if len(sys.argv) > 2 else "tai_leapsecs_dat.h"

leapsecs = parse_leap_seconds(input_file)

with open(output_file, 'w') as f:
    f.write("/* Auto-generated from leap-seconds.list */\n")
    f.write("#ifndef TAI_LEAPSECS_DAT_H\n#define TAI_LEAPSECS_DAT_H\n\n")
    f.write("#define LEAPSECS_MAX 256\n")
    f.write(f"#define LEAPSECS_INIT_COUNT {len(leapsecs)}\n\n")
    f.write("static struct tai leapsecs_table[LEAPSECS_MAX] = {\n")

    for i, (ntp_ts, leap_count) in enumerate(leapsecs):
        tai64 = ntp_ts - NTP_TO_UNIX + TAI64_BASE
        f.write(f"  {{ .x = 0x{tai64:016x}ULL }}")
        if i < len(leapsecs) - 1:
            f.write(",")
        f.write(f"  /* TAI-UTC = {leap_count}s */\n")

    f.write("  /* Remaining entries initialized to zero */\n")
    f.write("};\n\n")
    f.write("#endif\n")

print(f"Generated {output_file} with {len(leapsecs)} leap seconds")
