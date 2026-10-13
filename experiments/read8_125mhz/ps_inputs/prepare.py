"""Generate compact independent CPU oracle data for the authorized 100 MHz test.

No DUT/ROM imports: original full-table direction and independent NumPy pixels.
Outputs belong to a fresh board job, never to the immutable publication evidence.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
import numpy as np


def prepare(root, output, frame):
    """Build eight exact layers and final capacity-1000 feature bytes."""
    sys.path.insert(0, str(root / 'tests'))
    from oracle import WIDTHS, HEIGHTS, BUDGETS, front, gaussian, resize
    from prepare_descriptor import constants, describe, bytes_read
    source = root / 'optimized/FPGA_ORB_SHARED_v1.0/vectors/flight_20261004T184929_998838_6bb93c56_f0000/input.pgm'
    data = source.read_bytes()
    # Frozen fixture is binary P5, with no comment lines; reject other headers.
    head, dimensions, maximum, payload = data.split(b'\n', 3)
    assert (head, dimensions, maximum, len(payload)) == (b'P5', b'960 720', b'255', 691200)
    if frame == 'noise':
        state = 0x60A32026
        payload = bytearray(691200)
        for i in range(len(payload)):
            state = (state * 1664525 + 1013904223) & 0xffffffff
            payload[i] = state >> 24
    elif frame == 'zero':
        payload = bytes(691200)
    raw = np.frombuffer(payload, dtype=np.uint8).reshape(720, 960)
    table_path = root / 'optimized/FPGA_ORB_SHARED_v1.0/constants.hpp'
    tables = constants(table_path)
    blob = bytearray(4096)
    records = bytearray()
    rows = []

    def append(value):
        """Append cache-line-aligned bytes and return the relative DDR offset."""
        offset = len(blob)
        blob.extend(value)
        blob.extend(bytes((-len(blob)) % 64))
        return offset

    for level in range(8):
        summary, points = front(raw, (level, -1, 20, 7, 1, 1000, 0))
        blur = gaussian(raw)
        status, features, _ = describe(raw.tobytes(), blur.tobytes(), WIDTHS[level], HEIGHTS[level], points, level, tables)
        assert status == 0 and len(features) == len(points) <= 256
        raw_bytes, blur_bytes = bytes_read(WIDTHS[level], HEIGHTS[level], points)
        point_data = bytearray(b'\xa5' * 16000)
        for i, point in enumerate(points):
            struct.pack_into('<4i', point_data, i * 16, *point)
        row = [WIDTHS[level], HEIGHTS[level], len(points), len(records)//64,
               append(raw.tobytes() + bytes((-raw.size) % 8)),
               append(blur.tobytes() + bytes((-blur.size) % 8)),
               append(point_data), append(struct.pack('<I12i3I', *summary)),
               raw_bytes, blur_bytes, summary[2], BUDGETS[level]]
        rows.append(row)
        records.extend(b''.join(features))
        if level < 7:
            raw = resize(raw, level)
    expected = {'normal': 280, 'noise': 942, 'zero': 0}[frame]
    assert len(records)//64 == expected
    final_offset = append(records + b'\xc3' * (64000-len(records)))
    struct.pack_into('<8I', blob, 0, 0x60B30001, 1, len(blob), expected,
                     final_offset, sum(row[10] for row in rows), 8, 1000)
    for level, row in enumerate(rows):
        struct.pack_into('<12I', blob, 64 + 48*level, *row)
    output.mkdir(parents=True, exist_ok=True)
    (output/'golden.bin').write_bytes(blob)
    result = dict(frame=frame, features=expected, layers=rows, bytes=len(blob),
                  golden_sha256=hashlib.sha256(blob).hexdigest(),
                  input_sha256=hashlib.sha256(data).hexdigest(),
                  constants_sha256=hashlib.sha256(table_path.read_bytes()).hexdigest(),
                  scope='Independent CPU expected bytes, not hardware acceptance')
    (output/'golden.json').write_text(json.dumps(result, indent=2)+'\n')
    print('ORB60_CPU_GOLDEN_READY', frame, expected, len(blob), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--frame', choices=('normal', 'noise', 'zero'), default='normal')
    args = parser.parse_args()
    prepare(args.root, args.output, args.frame)
