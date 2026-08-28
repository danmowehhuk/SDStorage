#!/bin/bash

# Usage:
#   ./build.sh        Build and upload to real hardware
#   ./build.sh -s     Build a .hex suitable for SimulIDE simulation
#
# --library pins this sketch's SDStorage dependency to the copy this
# script lives under (two directories up), not whatever arduino-cli
# would otherwise discover under ~/Arduino/libraries - important when
# running from a worktree, since arduino-cli silently prefers
# ~/Arduino/libraries/SDStorage over the worktree copy unless told
# otherwise.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBRARY_ROOT="$(cd "$DIR/../.." && pwd)"

SIM_MODE=false
while getopts "s" opt; do
  case $opt in
    s) SIM_MODE=true ;;
  esac
done

COMPILE_CMD="arduino-cli compile -e -b arduino:avr:mega \
  --libraries ~/Arduino/libraries --library \"$LIBRARY_ROOT\" --clean \
  --build-property build.extra_flags=\"-DDEBUG\""

if $SIM_MODE; then
  # Capture verbose output to extract the avr-objcopy path, but still display it
  VERBOSE_LOG=$(mktemp)
  eval "$COMPILE_CMD --verbose \"$DIR\"" 2>&1 | tee "$VERBOSE_LOG"
  rm -f "$VERBOSE_LOG"
else
  eval "$COMPILE_CMD \"$DIR\""
  arduino-cli upload -b arduino:avr:mega -p /dev/cu.usbmodem1101 "$DIR"
fi

if $SIM_MODE; then
  # Convert type 02 (Extended Segment Address) records to type 04
  # (Extended Linear Address) records, which SimulIDE accepts but the
  # AVR toolchain's default ihex output does not produce.
  HEX=$(ls "$DIR"/build/arduino.avr.mega/*.hex | grep -v bootloader | head -1)
  SIM_HEX="${HEX%.hex}.sim.hex"
  python3 - "$HEX" "$SIM_HEX" << 'EOF'
import sys

def checksum(data_bytes):
    return (0x100 - sum(data_bytes) % 0x100) % 0x100

with open(sys.argv[1]) as f_in, open(sys.argv[2], 'w') as f_out:
    for line in f_in:
        line = line.strip()
        if line[7:9] == '02':  # Extended Segment Address record
            segment = int(line[9:13], 16)
            upper16 = segment >> 12
            b = [0x02, 0x00, 0x00, 0x04, upper16 >> 8, upper16 & 0xFF]
            f_out.write(f':{b[0]:02X}{b[1]:02X}{b[2]:02X}{b[3]:02X}{b[4]:02X}{b[5]:02X}{checksum(b):02X}\n')
        else:
            f_out.write(line + '\n')
EOF
  echo "SimulIDE-compatible hex: $SIM_HEX"
fi
