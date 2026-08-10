#!/usr/bin/env python3
"""Ad-hoc extractor for EA BIG archive entries (read-only inspection tool).

Usage: extract_big_entry.py <big_file> <entry_name_substring> [out_dir]

Parses the BIGF/BIG4 header (big-endian offset/size fields, entries listed
as offset:size:nul-terminated-name) and dumps any entry whose name contains
the given substring (case-insensitive) to out_dir (default: /tmp).
"""
import struct
import sys
import os


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    big_path = sys.argv[1]
    needle = sys.argv[2].lower()
    out_dir = sys.argv[3] if len(sys.argv) > 3 else "/tmp"

    with open(big_path, "rb") as f:
        data = f.read()

    magic = data[0:4]
    if magic not in (b"BIGF", b"BIG4"):
        print(f"Unexpected magic: {magic!r}")
        sys.exit(1)

    archive_size = struct.unpack(">I", data[4:8])[0]
    num_entries = struct.unpack(">I", data[8:12])[0]
    first_offset = struct.unpack(">I", data[12:16])[0]

    print(f"magic={magic} archive_size={archive_size} num_entries={num_entries} first_offset={first_offset}")

    pos = 16
    found = []
    for i in range(num_entries):
        entry_offset, entry_size = struct.unpack(">II", data[pos:pos + 8])
        pos += 8
        name_start = pos
        name_end = data.index(b"\x00", name_start)
        name = data[name_start:name_end].decode("latin-1")
        pos = name_end + 1

        if needle in name.lower():
            found.append((name, entry_offset, entry_size))

    if not found:
        print("No matching entries found.")
        return

    os.makedirs(out_dir, exist_ok=True)
    for name, off, size in found:
        payload = data[off:off + size]
        safe_name = name.replace("\\", "_").replace("/", "_")
        out_path = os.path.join(out_dir, safe_name)
        with open(out_path, "wb") as out:
            out.write(payload)
        print(f"Extracted {name} ({size} bytes) -> {out_path}")


if __name__ == "__main__":
    main()
