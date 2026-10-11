"""Compute PS/PL board-check digests from one schema-2 CPU reference trace."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def hash_bytes(value: int, payload: bytes) -> int:
    """Append raw bytes to a 64-bit FNV-1a digest."""
    for byte in payload:
        value = ((value ^ byte) * FNV_PRIME) & MASK64
    return value


def hash_u32(value: int, word: int) -> int:
    """Append one 32-bit integer in the accelerator's little-endian order."""
    return hash_bytes(value, (word & 0xFFFFFFFF).to_bytes(4, "little"))


def read_csv(path: Path) -> list[dict[str, str]]:
    """Read a reference CSV while preserving its frozen row order."""
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def digest_level(trace: Path, metadata: dict[str, int]) -> tuple[int, int]:
    """Return the pyramid-image and full externally visible level digests."""
    level = metadata["level"]
    prefix = f"L{level}"
    image_digest = hash_bytes(FNV_OFFSET, (trace / f"{prefix}_pyramid.u8").read_bytes())
    digest = hash_bytes(FNV_OFFSET, (trace / f"{prefix}_fast.u8").read_bytes())
    digest = hash_bytes(digest, (trace / f"{prefix}_gaussian.u8").read_bytes())

    for field in (
        "candidates",
        "selected",
        "cell_side",
        "cell_cols",
        "cell_rows",
        "empty_cells",
        "fallback_cells",
    ):
        digest = hash_u32(digest, metadata[field])
    digest = hash_u32(digest, 0x00010000)
    digest = hash_u32(digest, 0x00000001)
    digest = hash_u32(digest, 1)

    candidates = read_csv(trace / f"{prefix}_nms.csv")
    selected = read_csv(trace / f"{prefix}_grid.csv")
    moments = read_csv(trace / f"{prefix}_moments.csv")
    descriptors = (trace / f"{prefix}_descriptors.u8").read_bytes()
    if len(candidates) != metadata["candidates"]:
        raise ValueError(f"{prefix}: candidate count mismatch")
    if len(selected) != metadata["selected"] or len(moments) != len(selected):
        raise ValueError(f"{prefix}: selected/moment count mismatch")
    if len(descriptors) != 32 * len(selected):
        raise ValueError(f"{prefix}: descriptor size mismatch")

    for point in candidates:
        for field in ("x_effective", "y_effective", "response", "tile"):
            digest = hash_u32(digest, int(point[field]))

    for index, point in enumerate(selected):
        moment = moments[index]
        for field in ("x_effective", "y_effective", "response", "tile"):
            digest = hash_u32(digest, int(point[field]))
        digest = hash_u32(digest, int(moment["x_layer"]))
        digest = hash_u32(digest, int(moment["y_layer"]))
        digest = hash_u32(digest, int(point["response"]))
        digest = hash_u32(digest, level)
        digest = hash_u32(digest, int(moment["bin"]))
        digest = hash_u32(digest, int(moment["m10"]))
        digest = hash_u32(digest, int(moment["m01"]))
        digest = hash_u32(digest, int(moment["angle_q16"]))
        descriptor = descriptors[index * 32 : (index + 1) * 32]
        for word_offset in range(0, 32, 4):
            digest = hash_u32(digest, int.from_bytes(descriptor[word_offset : word_offset + 4], "little"))
    return image_digest, digest


def main() -> int:
    """Parse one trace, validate its profile, and print C-ready golden constants."""
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    arguments = parser.parse_args()
    document = json.loads((arguments.trace / "trace.json").read_text(encoding="utf-8"))
    if document.get("schema") != 2 or len(document.get("levels", [])) != 8:
        raise ValueError("expected one schema-2, eight-level trace")
    for metadata in document["levels"]:
        image_digest, level_digest = digest_level(arguments.trace, metadata)
        print(
            f"L{metadata['level']}: image=0x{image_digest:016x}, "
            f"level=0x{level_digest:016x}, candidates={metadata['candidates']}, "
            f"selected={metadata['selected']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
