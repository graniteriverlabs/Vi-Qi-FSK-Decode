#!/usr/bin/env python3
from __future__ import annotations
import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

@dataclass
class Segment:
    path: Path
    offset: int
    data: bytes

def parse_int(x: str) -> int:
    return int(x, 0)  # supports 0x... or decimal

def load_segments(seg_args: List[Tuple[str, str]]) -> List[Segment]:
    segs: List[Segment] = []
    for file_str, off_str in seg_args:
        p = Path(file_str).expanduser().resolve()
        if not p.exists():
            raise FileNotFoundError(f"Input not found: {p}")
        off = parse_int(off_str)
        if off < 0:
            raise ValueError(f"Offset must be >= 0, got {off_str}")
        segs.append(Segment(path=p, offset=off, data=p.read_bytes()))
    return segs

def merge(segs: List[Segment], pad: int = 0xFF, max_size: int | None = None, pad_to: int | None = None) -> bytes:
    if not segs:
        raise ValueError("Provide at least one --seg <file> <offset>")

    end = max(s.offset + len(s.data) for s in segs)

    if pad_to is not None:
        if pad_to < end:
            raise ValueError(f"--pad-to 0x{pad_to:X} is smaller than needed size 0x{end:X}")
        end = pad_to

    if max_size is not None and end > max_size:
        raise ValueError(f"Merged image size 0x{end:X} exceeds --max-size 0x{max_size:X}")

    out = bytearray([pad]) * end

    # Place segments (reject overlaps)
    for s in sorted(segs, key=lambda x: x.offset):
        start = s.offset
        stop = start + len(s.data)

        # overlap check: anything not pad is already written
        for i in range(start, stop):
            if out[i] != pad:
                raise ValueError(
                    f"Overlap detected placing {s.path.name} at offset 0x{s.offset:X} "
                    f"(collision at output offset 0x{i:X})."
                )

        out[start:stop] = s.data

    return bytes(out)

def main() -> None:
    ap = argparse.ArgumentParser(description="Merge STM32 .bin files into a single OTA image using OFFSETS.")
    ap.add_argument("-o", "--out", required=True, help="Output merged .bin (OTA image)")
    ap.add_argument("--seg", action="append", nargs=2, metavar=("FILE.bin", "OFFSET"),
                    required=True, help="Repeatable: --seg <file.bin> <offset>")
    ap.add_argument("--pad", default="0xFF", help="Pad byte for gaps (default 0xFF)")
    ap.add_argument("--max-size", default=None, help="Optional max output size (e.g. 0x100000 for 1MB bank)")
    ap.add_argument("--pad-to", default=None, help="Optional: pad output to exact size (e.g. 0x100000)")

    args = ap.parse_args()

    pad = parse_int(args.pad)
    if not (0 <= pad <= 0xFF):
        raise ValueError("--pad must be 0..255")

    max_size = parse_int(args.max_size) if args.max_size is not None else None
    pad_to = parse_int(args.pad_to) if args.pad_to is not None else None

    segs = load_segments(args.seg)
    merged = merge(segs, pad=pad, max_size=max_size, pad_to=pad_to)

    out_path = Path(args.out).expanduser().resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(merged)

    print(f"Wrote: {out_path}")
    print(f"Size : {len(merged)} bytes (0x{len(merged):X})")
    print("Segments:")
    for s in sorted(segs, key=lambda x: x.offset):
        print(f"  - {s.path.name:20s}  offset=0x{s.offset:X}  size=0x{len(s.data):X}")

if __name__ == "__main__":
    main()
